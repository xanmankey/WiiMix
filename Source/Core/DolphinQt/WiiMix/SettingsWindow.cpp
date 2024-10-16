// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/WiiMix/SettingsWindow.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMessageBox>

#include "Core/ConfigLoaders/GameConfigLoader.h"

#include "Common/IniFile.h"
#include "Common/FileUtil.h"

#include "DolphinQt/QtUtils/WrapInScrollArea.h"
#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/WiiMix/Enums.h"
#include "DolphinQt/WiiMix/ModesWidget.h"
#include "DolphinQt/WiiMix/ConfigWidget.h"
#include "DolphinQt/WiiMix/BingoSettings.h"
#include "DolphinQt/WiiMix/RogueSettings.h"
#include "DolphinQt/WiiMix/ShuffleSettings.h"
#include <iostream>

WiiMixSettingsWindow::WiiMixSettingsWindow(QWidget *parent) : QDialog(parent)
{
  setWindowTitle(tr("WiiMix"));

  m_modes = new WiiMixModesWidget(this);
  m_load_button_box = new QPushButton(tr("Load"));
  // m_load_button_box->button(QDialogButtonBox::Open)->setText(tr("Load"));
  // m_save_button_box = new QDialogButtonBox(QDialogButtonBox::Save);
  m_save_button_box = new QPushButton(tr("Save"));
  // m_load_button_box->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  // m_save_button_box->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  m_wii_mix_button = new QPushButton();
  m_wii_mix_button->setIcon(Resources::GetResourceIcon("wiimix_text"));
  m_wii_mix_button->setIconSize(QSize(128, 128));  // Adjust this size as needed
  m_wii_mix_button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  CreateMainLayout();
  ConnectWidgets();
  return;
}

// TODO: Maybe will want this in case there's a settings mismatch between people
// trying to play together
void WiiMixSettingsWindow::showEvent(QShowEvent* event)
{
//   QDialog::showEvent(event);
//   m_wiimote_controllers->UpdateBluetoothAvailableStatus();
    return;
}

void WiiMixSettingsWindow::CreateMainLayout()
{
  auto* layout = new QVBoxLayout();

  layout->addWidget(m_modes);

  QHBoxLayout* bottom_buttons = new QHBoxLayout();
  bottom_buttons->setSpacing(2);  // Set spacing between widgets
  bottom_buttons->setContentsMargins(0, 0, 0, 0);  // Remove any margins around the buttons
  bottom_buttons->addStretch(); // Add space before the buttons
  bottom_buttons->addWidget(m_load_button_box, 0);
  bottom_buttons->addWidget(m_wii_mix_button, 0);
  bottom_buttons->addWidget(m_save_button_box, 0);
  bottom_buttons->addStretch(); // Add space after the buttons

  layout->addLayout(bottom_buttons);

  // WrapInScrollArea(this, layout);
  setLayout(layout);
  return;
}

void WiiMixSettingsWindow::ConnectWidgets()
{
    connect(m_modes, &WiiMixModesWidget::ModeChanged, this, &WiiMixSettingsWindow::CreateLayout);
    connect(m_load_button_box, &QPushButton::clicked, this, &WiiMixSettingsWindow::LoadSettings);
    connect(m_save_button_box, &QPushButton::clicked, this, &WiiMixSettingsWindow::SaveSettings);
    connect(m_wii_mix_button, &QPushButton::clicked, this, [this] {
      if (m_config) {
        m_settings.SetDifficulty(WiiMixSettings::StringToDifficulty(m_config->GetDifficulty()));
        m_settings.SetSaveStateBank(WiiMixSettings::StringToSaveStateBank(m_config->GetSaveStateBank()));
        if (m_settings.GetMode() == WiiMixEnums::Mode::BINGO) {
          WiiMixBingoSettings bingo_settings = WiiMixBingoSettings(m_settings);
          bingo_settings.SetCardSize(WiiMixSettings::StringToCardSize(m_config->GetCardSize()));
          bingo_settings.SetLockout(m_config->GetIsLockout());
          emit StartWiiMixBingo(bingo_settings);
        }
        else if (m_settings.GetMode() == WiiMixEnums::Mode::SHUFFLE) {
          WiiMixShuffleSettings shuffle_settings = WiiMixShuffleSettings(m_settings);
          shuffle_settings.SetNumberOfSwitches(m_config->GetNumSwitches());
          shuffle_settings.SetMinTimeBetweenSwitch(m_config->GetMinTimeBetweenSwitch());
          shuffle_settings.SetMaxTimeBetweenSwitch(m_config->GetMaxTimeBetweenSwitch());
          shuffle_settings.SetEndless(m_config->GetEndless());
          emit StartWiiMixShuffle(shuffle_settings);
        }
        else if (m_settings.GetMode() == WiiMixEnums::Mode::ROGUE) {
          WiiMixRogueSettings rogue_settings = WiiMixRogueSettings(m_settings);
          emit StartWiiMixRogue(rogue_settings);
        }
      }
    }); // Start WiiMix
    connect(this, &WiiMixSettingsWindow::ErrorLoadingSettings, this, [this](const QString& error_message) {
      // Display Error message
      QMessageBox::critical(this, tr("Error"), error_message);
    });
}

void WiiMixSettingsWindow::CreateLayout(WiiMixEnums::Mode mode)
{
  m_config = new WiiMixConfigWidget(this);
  m_settings.SetMode(mode);
  m_config->CreateLayout(mode);

  // TODOx REMOVE THIS ITS ONLY FOR TESTING
  m_settings.AddGame(UICommon::GameFile("/Users/scie/Documents/dolphinjank/iso/NewSuperMarioBrosWii.wbfs"));
  m_settings.AddGame(UICommon::GameFile("/Users/scie/Documents/dolphinjank/iso/Wii Sports (USA) (Rev 1).wbfs"));




  // Create a new window with the configuration options
  auto* config_window = new QDialog(this);
  config_window->setWindowTitle(tr("Configuration Options"));
  auto* config_layout = new QVBoxLayout(config_window);
  config_layout->addWidget(m_config);
  config_window->setLayout(config_layout);
  config_window->exec();
  return;
}

void WiiMixSettingsWindow::LoadSettings() {
  auto& settings = Settings::Instance().GetQSettings();
  QString path = DolphinFileDialog::getOpenFileName(
      this, tr("Select a File"),
      settings.value(QStringLiteral("mainwindow/lastdir"), QString{}).toString(),
      QStringLiteral("%1 (*.ini)")
          .arg(tr("Configuration file (*.ini)")));

  if (!path.isEmpty()) {
    // Load the settings from the file
    Common::IniFile ini = Common::IniFile();
    ini.Load(path.toStdString());
    // Load the settings into the config widget
    // Erroring out if any of the settings are missing
    if (!ini.GetSection("WiiMix")) {
      return;
    }
    Common::IniFile::Section *section = ini.GetSection("WiiMix");
    std::string *val;

    section->Get("Games", val, "");
    if (val->empty()) {
      emit ErrorLoadingSettings(QStringLiteral("Games not found in WiiMix settings file"));
      return;
    }
    std::vector<UICommon::GameFile> games = WiiMixSettings::GameIdsToGameFiles(val->c_str());
    if (games.size() != 0) {
      m_settings.SetGamesList(games);
    }
    else {
      emit ErrorLoadingSettings(QStringLiteral("Games in config not found in game list"));
      return;
    }
    m_settings.SetGamesList(games);
    Config::Set(Config::LayerType::Base, Config::WIIMIX_IS_ENDLESS, val->c_str());

    section->Get("Mode", val, "");
    if (val->empty()) {
      emit ErrorLoadingSettings(QStringLiteral("Mode not found in WiiMix settings file"));
      return;
    }
    m_settings.SetMode(WiiMixSettings::StringToMode(QString::fromStdString(val->c_str())));
    Config::Set(Config::LayerType::Base, Config::WIIMIX_MODE, m_settings.GetMode());

    section->Get("Difficulty", val, "");
    if (val->empty()) {
      // Emit error
      emit ErrorLoadingSettings(QStringLiteral("Difficulty not found in WiiMix settings file"));
      return;
    }
    m_settings.SetDifficulty(WiiMixSettings::StringToDifficulty(QString::fromStdString(val->c_str())));
    Config::Set(Config::LayerType::Base, Config::WIIMIX_DIFFICULTY, m_settings.GetDifficulty());

    section->Get("Objectives", val, "");
    if (val->empty()) {
      emit ErrorLoadingSettings(QStringLiteral("Objectives not found in WiiMix settings file"));
      return;
    }
    std::vector<WiiMixObjective> objectives = WiiMixSettings::ObjectiveIdsToObjectives(val->c_str());
    if (objectives.size() != 0) {
      m_settings.SetObjectives(objectives);
    }
    else {
      emit ErrorLoadingSettings(QStringLiteral("Invalid or removed objectives found in objective list"));
      return;
    }
    m_settings.SetObjectives(objectives);
    Config::Set(Config::LayerType::Base, Config::WIIMIX_OBJECTIVE_IDS, val->c_str());

    section->Get("SaveStateBank", val, "");
    if (val->empty()) {
      emit ErrorLoadingSettings(QStringLiteral("Save State Bank not found in WiiMix settings file"));
      return;
    }
    m_settings.SetSaveStateBank(WiiMixSettings::StringToSaveStateBank(QString::fromStdString(val->c_str())));
    Config::Set(Config::LayerType::Base, Config::WIIMIX_SAVE_STATE_BANK, m_settings.GetSaveStateBank());

    WiiMixBingoSettings bingo_settings = WiiMixBingoSettings(m_settings);
    WiiMixRogueSettings rogue_settings = WiiMixRogueSettings(m_settings);
    WiiMixShuffleSettings shuffle_settings = WiiMixShuffleSettings(m_settings);
    if (m_settings.GetMode() == WiiMixEnums::Mode::BINGO) {
      section->Get("Lockout", val, "");
      if (val->empty()) {
        emit ErrorLoadingSettings(QStringLiteral("Lockout not found in WiiMix settings file"));
        return;
      }
      bingo_settings.SetLockout(val->c_str() == std::string("true"));
      Config::Set(Config::LayerType::Base, Config::WIIMIX_IS_LOCKOUT, bingo_settings.GetLockout());

      section->Get("CardSize", val, "");
      if (val->empty()) {
        emit ErrorLoadingSettings(QStringLiteral("Card Size not found in WiiMix settings file"));
        return;
      }
      bingo_settings.SetCardSize(WiiMixSettings::StringToCardSize(QString::fromStdString(val->c_str())));
      Config::Set(Config::LayerType::Base, Config::WIIMIX_CARD_SIZE, bingo_settings.GetCardSize());
    }
    else if (m_settings.GetMode() == WiiMixEnums::Mode::SHUFFLE) {
      section->Get("NumberOfSwitches", val, "");
      if (val->empty()) {
        emit ErrorLoadingSettings(QStringLiteral("Number of Switches not found in WiiMix settings file"));
        return;
      }
      shuffle_settings.SetNumberOfSwitches(std::stoi(*val));
      Config::Set(Config::LayerType::Base, Config::WIIMIX_NUMBER_OF_SWITCHES, shuffle_settings.GetNumberOfSwitches());

      section->Get("MinTimeBetweenSwitch", val, "");
      if (val->empty()) {
        emit ErrorLoadingSettings(QStringLiteral("Min Time Between Switch not found in WiiMix settings file"));
        return;
      }
      shuffle_settings.SetMinTimeBetweenSwitch(std::stoi(*val));
      Config::Set(Config::LayerType::Base, Config::WIIMIX_MIN_TIME_BETWEEN_SWITCH, shuffle_settings.GetMinTimeBetweenSwitch());

      section->Get("MaxTimeBetweenSwitch", val, "");
      if (val->empty()) {
        emit ErrorLoadingSettings(QStringLiteral("Max Time Between Switch not found in WiiMix settings file"));
        return;
      }
      shuffle_settings.SetMaxTimeBetweenSwitch(std::stoi(*val));
      Config::Set(Config::LayerType::Base, Config::WIIMIX_MAX_TIME_BETWEEN_SWITCH, shuffle_settings.GetMaxTimeBetweenSwitch());

      section->Get("Endless", val, "");
      if (val->empty()) {
        emit ErrorLoadingSettings(QStringLiteral("Endless not found in WiiMix settings file"));
        return;
      }
      shuffle_settings.SetEndless(val->c_str() == std::string("true"));
      Config::Set(Config::LayerType::Base, Config::WIIMIX_IS_ENDLESS, shuffle_settings.GetEndless());
    }
    // else if (m_settings.GetMode() == WiiMixEnums::Mode::ROGUE) {

    // }
  }
}

void WiiMixSettingsWindow::SaveSettings() {
  // TODOx: Load objectives
  if (m_settings.GetObjectives().size() == 0) {
    m_settings.SetObjectives({});
  }

  // Load games
  if (m_settings.GetGamesList().size() == 0) {
    // set the games list based on the games enabled for WiiMix
    // Looks at the game specific config files
    std::vector<UICommon::GameFile> games;
    Common::IniFile game_ini;
    for (const auto& entry : std::filesystem::directory_iterator(File::GetUserPath(D_GAMESETTINGS_IDX)))
    {
      game_ini.Load(entry.path().string(), false);
      std::string game_id = entry.path().string().substr(entry.path().string().find_last_of('/') + 1);
      game_id = game_id.substr(0, game_id.find_last_of('.'));
      if (game_ini.GetSection("WiiMix")) {
        bool enabled = false;
        game_ini.GetSection("WiiMix")->Get("WiiMix", &enabled);
        if (enabled == true) {
          UICommon::GameFile game;
          // TODOx: GameFile::GetGameFileById(game_id, game) doesn't work
          UICommon::GameFile::GetGameFileById(game_id, &game);
          if (game.IsValid()) {
            games.push_back(game);
          }
          else {
            // Error out if the game is not found
            emit ErrorLoadingSettings(QStringLiteral("Game %1 not found in game list").arg(QString::fromStdString(game_id)));
            return;
          }
        }
      }
    }
    m_settings.SetGamesList(games);
  }

  // Set all config values
  auto& settings = Settings::Instance().GetQSettings();
  QString path = DolphinFileDialog::getSaveFileName(
      this, tr("Save File"),
      settings.value(QStringLiteral("mainwindow/lastdir"), QString{}).toString(),
      QStringLiteral("%1 (*.ini)")
          .arg(tr("Configuration file (*.ini)")));

  if (!path.endsWith(QStringLiteral(".ini"), Qt::CaseInsensitive)) {
    path += QStringLiteral(".ini");
  }

  if (!path.isEmpty()) {
    Common::IniFile ini = Common::IniFile();
    auto* section = ini.GetOrCreateSection("WiiMix");

    section->Set("Games", WiiMixSettings::GameFilesToGameIds(m_settings.GetGamesList()));
    Config::Set(Config::LayerType::Base, Config::WIIMIX_GAME_IDS, WiiMixSettings::GameFilesToGameIds(m_settings.GetGamesList()));
    section->Set("Mode", WiiMixSettings::ModeToTitle(m_settings.GetMode()).toStdString());
    Config::Set(Config::LayerType::Base, Config::WIIMIX_MODE, m_settings.GetMode());
    section->Set("Difficulty", WiiMixSettings::DifficultyToString(m_settings.GetDifficulty()).toStdString());
    Config::Set(Config::LayerType::Base, Config::WIIMIX_DIFFICULTY, m_settings.GetDifficulty());
    section->Set("Objectives", WiiMixSettings::ObjectivesToObjectiveIds(m_settings.GetObjectives()));
    Config::Set(Config::LayerType::Base, Config::WIIMIX_OBJECTIVE_IDS, WiiMixSettings::ObjectivesToObjectiveIds(m_settings.GetObjectives()));

    section->Set("SaveStateBank", WiiMixSettings::SaveStateBankToString(m_settings.GetSaveStateBank()).toStdString());
    Config::Set(Config::LayerType::Base, Config::WIIMIX_SAVE_STATE_BANK, m_settings.GetSaveStateBank());

    if (m_settings.GetMode() == WiiMixEnums::Mode::BINGO) {
      WiiMixBingoSettings bingo_settings = WiiMixBingoSettings(m_settings);
      section->Set("Lockout", bingo_settings.GetLockout());
      Config::Set(Config::LayerType::Base, Config::WIIMIX_IS_LOCKOUT, bingo_settings.GetLockout());

      section->Set("CardSize", bingo_settings.GetCardSize());
      Config::Set(Config::LayerType::Base, Config::WIIMIX_CARD_SIZE, bingo_settings.GetCardSize());
    }
    else if (m_settings.GetMode() == WiiMixEnums::Mode::SHUFFLE) {
      WiiMixShuffleSettings shuffle_settings = WiiMixShuffleSettings(m_settings);
      section->Set("NumberOfSwitches", shuffle_settings.GetNumberOfSwitches());
      Config::Set(Config::LayerType::Base, Config::WIIMIX_NUMBER_OF_SWITCHES, shuffle_settings.GetNumberOfSwitches());

      section->Set("MinTimeBetweenSwitch", shuffle_settings.GetMinTimeBetweenSwitch());
      Config::Set(Config::LayerType::Base, Config::WIIMIX_MIN_TIME_BETWEEN_SWITCH, shuffle_settings.GetMinTimeBetweenSwitch());

      section->Set("MaxTimeBetweenSwitch", shuffle_settings.GetMaxTimeBetweenSwitch());
      Config::Set(Config::LayerType::Base, Config::WIIMIX_MAX_TIME_BETWEEN_SWITCH, shuffle_settings.GetMaxTimeBetweenSwitch());

      section->Set("Endless", shuffle_settings.GetEndless());
      Config::Set(Config::LayerType::Base, Config::WIIMIX_IS_ENDLESS, shuffle_settings.GetEndless());
    }
    // else if (m_settings.GetMode() == WiiMixEnums::Mode::ROGUE) {

    // }

    ini.Save(path.toStdString());
  }
  // Config::Get(Config::WIIMIX_GAME_IDS);
  // Config::Get(Config::WIIMIX_OBJECTIVE_IDS);
  // Config::Get(Config::WIIMIX_DIFFICULTY);
  // Config::Get(Config::WIIMIX_MODE);
  // Config::Get(Config::WIIMIX_SAVE_STATE_BANK);
  // Config::Get(Config::WIIMIX_IS_LOCKOUT);
  // Config::Get(Config::WIIMIX_CARD_SIZE);
  // Config::Get(Config::WIIMIX_NUMBER_OF_SWITCHES);
  // Config::Get(Config::WIIMIX_MIN_TIME_BETWEEN_SWITCH);
  // Config::Get(Config::WIIMIX_MAX_TIME_BETWEEN_SWITCH);
  // Config::Get(Config::WIIMIX_IS_ENDLESS);
  // if (m_settings.GetObjectives().size() == 0) {

  // }
  // // Creates games if it is currently empty
  // if (m_settings.GetGamesList().size() == 0) {
  //   for () {

  //   }
  // }

  // Save to ini file with the settings
}

std::vector<UICommon::GameFile> WiiMixSettingsWindow::GetGamesList() {
  return m_settings.GetGamesList();
}
