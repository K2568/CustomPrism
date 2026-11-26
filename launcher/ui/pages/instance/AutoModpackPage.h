// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "ModFolderPage.h"

class QButtonGroup;
class QEvent;
class QHBoxLayout;
class QLineEdit;
class QObject;

class AutoModpackPage : public ModFolderPage {
    Q_OBJECT

   public:
    explicit AutoModpackPage(MinecraftInstance* instance, QWidget* parent = nullptr);

    QString displayName() const override { return tr("AutoModpack"); }
    QIcon icon() const override { return QIcon::fromTheme("loadermods"); }
    QString id() const override { return "automodpack"; }
    QString helpPage() const override { return "Loader-mods"; }

   protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

   private:
    void selectPack(const QString& packName);
    void setPackButtons();

    QString m_modpacksRoot;
    QLineEdit* m_folderEdit = nullptr;
    QButtonGroup* m_packGroup = nullptr;
    QHBoxLayout* m_packLayout = nullptr;
};