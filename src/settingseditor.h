#pragma once

#include <QDialog>

#include "settings.h"

namespace Ui
{
class SettingsEditor;
}

class SettingsEditor : public QDialog
{
  Q_OBJECT

public:
  explicit SettingsEditor();
  ~SettingsEditor();

  Settings settings() const;
  void setSettings(const Settings &settings);

private:
  void updateCurrentPage();
  void openTessdataDialog();
  void updateTesseractLanguages();
  void updateCorrectionsTable();
  void updateTranslators(const QStringList &enabled);
  void updateTranslationLanguages();

  Ui::SettingsEditor *ui;
  QString translatorsDir_;
};
