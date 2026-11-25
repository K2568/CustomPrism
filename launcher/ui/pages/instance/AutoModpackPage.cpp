// SPDX-License-Identifier: GPL-3.0-only
#include "AutoModpackPage.h"

#include <QAction>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

#include "Application.h"
#include "minecraft/mod/ModFolderModel.h"
#include "settings/Setting.h"
#include "ui_ExternalResourcesPage.h"

namespace {
class AutoModpackFolderModel : public ModFolderModel {
   public:
    using ModFolderModel::ModFolderModel;

    QString id() const override { return "automodpack"; }

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

AutoModpackFolderModel* createAutoModpackModel(MinecraftInstance* instance)
{
    auto path = instance->settings()->get("AutoModpackDirectory").toString();
    if (path.isEmpty()) {
        path = "automodpack/modpacks";
    }
    if (QDir::isRelativePath(path)) {
        path = QDir(instance->gameRoot()).filePath(path);
    }

    const bool isIndexed = !APPLICATION->settings()->get("ModMetadataDisabled").toBool();
    return new AutoModpackFolderModel(QDir(path), instance, isIndexed, true);
}
}  // namespace

AutoModpackPage::AutoModpackPage(MinecraftInstance* instance, QWidget* parent)
    : ModFolderPage(instance, createAutoModpackModel(instance), parent)
{
    m_model->setParent(this);

    for (QAction* action : { m_ui->actionDownloadItem, m_ui->actionUpdateItem, m_ui->actionChangeVersion,
                             m_ui->actionLockUpdates, m_ui->actionUnlockUpdates, m_ui->actionAddItem,
                             m_ui->actionRemoveItem, m_ui->actionEnableItem, m_ui->actionDisableItem }) {
        action->setVisible(false);
    }

    auto* folderRow = new QWidget(m_ui->centralwidget);
    auto* folderLayout = new QHBoxLayout(folderRow);
    folderLayout->setContentsMargins(0, 0, 0, 0);

    auto* folderLabel = new QLabel(tr("Folder:"), folderRow);
    m_folderEdit = new QLineEdit(folderRow);
    m_folderEdit->setReadOnly(true);
    auto* browseButton = new QPushButton(tr("Browse..."), folderRow);

    folderLayout->addWidget(folderLabel);
    folderLayout->addWidget(m_folderEdit, 1);
    folderLayout->addWidget(browseButton);
    m_ui->gridLayout->addWidget(folderRow, 0, 1, 1, 2);

    setFolder(m_model->dir().absolutePath());
    connect(browseButton, &QPushButton::clicked, this, &AutoModpackPage::chooseFolder);
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

void AutoModpackPage::chooseFolder()
{
    const auto folder =
        QFileDialog::getExistingDirectory(this, tr("Select automodpack modpacks folder"), m_folderEdit->text());
    if (!folder.isEmpty()) {
        setFolder(folder);
    }
}

void AutoModpackPage::setFolder(const QString& folder)
{
    m_folderEdit->setText(QDir::toNativeSeparators(folder));
    m_instance->settings()->set("AutoModpackDirectory", folder);
    static_cast<AutoModpackFolderModel*>(m_model)->setDirectory(folder);
}
