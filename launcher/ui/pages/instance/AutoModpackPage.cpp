// SPDX-License-Identifier: GPL-3.0-only
#include "AutoModpackPage.h"

#include <QButtonGroup>
#include <QDir>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QToolButton>
#include <QWidget>

#include "Application.h"
#include "minecraft/mod/ModFolderModel.h"
#include "settings/Setting.h"
#include "ui_ExternalResourcesPage.h"

namespace {
QString getModpacksRoot(MinecraftInstance* instance)
{
    auto relativePath = APPLICATION->settings()->get("AutoModpackRelativePath").toString().trimmed();
    if (relativePath.isEmpty()) {
        relativePath = "automodpack/modpacks";
    }

    relativePath = QDir::cleanPath(relativePath);
    if (QDir::isAbsolutePath(relativePath) || relativePath == ".." || relativePath.startsWith("../")) {
        relativePath = "automodpack/modpacks";
    }

    return QDir(instance->gameRoot()).absoluteFilePath(relativePath);
}

QString getUnselectedModsPath(const QString& root)
{
    return QDir(root).filePath(".no-pack-selected/mods");
}

QStringList getPackFolders(const QString& root)
{
    QStringList folders;
    const QDir rootDir(root);

    for (const auto& entry : rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        if (QDir(entry.filePath("mods")).exists()) {
            folders.append(entry.fileName());
        }
    }

    return folders;
}

class AutoModpackFolderModel : public ModFolderModel {
   public:
    using ModFolderModel::ModFolderModel;

    QString id() const override { return "automodpack"; }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (index.column() == ActiveColumn && role == Qt::CheckStateRole) {
            return {};
        }
        return ModFolderModel::data(index, role);
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        auto itemFlags = ModFolderModel::flags(index);
        itemFlags &= ~Qt::ItemIsUserCheckable;
        return itemFlags;
    }

    bool setResourceEnabled(const QModelIndexList&, EnableAction) override { return false; }

    void setDirectory(const QString& path)
    {
        const auto normalizedPath = QDir::cleanPath(QDir(path).absolutePath());
        if (QDir::cleanPath(m_dir.absolutePath()) == normalizedPath) {
            return;
        }

        const bool wasWatching = m_isWatching;
        if (wasWatching) {
            stopWatching();
        }

        m_dir.setPath(normalizedPath);
        m_dir.setFilter(QDir::Readable | QDir::NoDotAndDotDot | QDir::Files);
        m_firstFolderLoad = true;

        if (wasWatching) {
            startWatching();
        } else {
            update();
        }
    }
};

AutoModpackFolderModel* createModel(MinecraftInstance* instance)
{
    const bool isIndexed = !APPLICATION->settings()->get("ModMetadataDisabled").toBool();

    // Do not create the configured root or this placeholder directory.
    return new AutoModpackFolderModel(QDir(getUnselectedModsPath(getModpacksRoot(instance))), instance, isIndexed, false);
}
}  // namespace

AutoModpackPage::AutoModpackPage(MinecraftInstance* instance, QWidget* parent)
    : ModFolderPage(instance, createModel(instance), parent)
    , m_modpacksRoot(getModpacksRoot(instance))
{
    m_model->setParent(this);

    for (auto* action : { m_ui->actionDownloadItem, m_ui->actionUpdateItem, m_ui->actionChangeVersion,
                          m_ui->actionLockUpdates, m_ui->actionUnlockUpdates, m_ui->actionAddItem,
                          m_ui->actionRemoveItem, m_ui->actionEnableItem, m_ui->actionDisableItem,
                          m_ui->actionResetItemMetadata, m_ui->actionVerifyItemDependencies }) {
        action->setVisible(false);
    }

    auto* directoryRow = new QWidget(m_ui->centralwidget);
    auto* directoryLayout = new QHBoxLayout(directoryRow);
    directoryLayout->setContentsMargins(0, 0, 0, 0);
    directoryLayout->addWidget(new QLabel(tr("Modpacks directory:"), directoryRow));

    m_folderEdit = new QLineEdit(directoryRow);
    m_folderEdit->setReadOnly(true);
    m_folderEdit->setText(QDir::toNativeSeparators(m_modpacksRoot));
    directoryLayout->addWidget(m_folderEdit, 1);

    auto* packSelector = new QWidget(m_ui->centralwidget);
    m_packLayout = new QHBoxLayout(packSelector);
    m_packLayout->setContentsMargins(0, 0, 0, 0);

    m_packGroup = new QButtonGroup(this);
    m_packGroup->setExclusive(true);

    auto* scrollArea = new QScrollArea(m_ui->centralwidget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setWidget(packSelector);

    auto* layout = m_ui->gridLayout;
    layout->removeWidget(m_ui->counts);
    layout->removeWidget(m_ui->treeView);
    layout->removeWidget(m_ui->frame);
    layout->removeWidget(m_ui->filterEdit);

    layout->addWidget(directoryRow, 0, 1, 1, 2);
    layout->addWidget(scrollArea, 1, 1, 1, 2);
    layout->addWidget(m_ui->counts, 2, 1);
    layout->addWidget(m_ui->treeView, 3, 1, 1, 2);
    layout->addWidget(m_ui->frame, 4, 1, 1, 2);
    layout->addWidget(m_ui->filterEdit, 5, 1);

    setPackButtons();

    const auto savedPack = instance->settings()->get("AutoModpackSelectedPack").toString();
    const auto folders = getPackFolders(m_modpacksRoot);

    QString packToSelect;
    if (folders.size() == 1) {
        packToSelect = folders.first();
    } else if (folders.contains(savedPack)) {
        packToSelect = savedPack;
    }

    if (!packToSelect.isEmpty()) {
        selectPack(packToSelect);
    }
}

void AutoModpackPage::setPackButtons()
{
    const auto folders = getPackFolders(m_modpacksRoot);

    for (const auto& folder : folders) {
        auto* button = new QToolButton(m_packGroup);
        button->setText(folder);
        button->setCheckable(true);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        m_packGroup->addButton(button);
        m_packLayout->addWidget(button);

        connect(button, &QToolButton::clicked, this, [this, folder] { selectPack(folder); });
    }
    m_packLayout->addStretch();
}

void AutoModpackPage::selectPack(const QString& packName)
{
    const auto folders = getPackFolders(m_modpacksRoot);
    if (!folders.contains(packName)) {
        return;
    }

    for (auto* button : m_packGroup->buttons()) {
        button->setChecked(button->text() == packName);
    }

    m_instance->settings()->set("AutoModpackSelectedPack", packName);

    const auto modsPath = QDir(m_modpacksRoot).filePath(packName + "/mods");
    static_cast<AutoModpackFolderModel*>(m_model)->setDirectory(modsPath);
}

bool AutoModpackPage::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == m_ui->treeView && ev->type() == QEvent::KeyPress) {
        const auto* keyEvent = static_cast<QKeyEvent*>(ev);
        if (keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Plus) {
            return true;
        }
    }

    return ModFolderPage::eventFilter(obj, ev);
}