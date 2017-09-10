#include "MainWindow.h"

#include <QHBoxLayout>
#include <QMenuBar>
#include <QMessageBox>
#include <QShortcut>
#include <QString>
#include <QVBoxLayout>
#include <string>

#include "../DolphinProcess/DolphinAccessor.h"
#include "../MemoryWatch/MemoryWatch.h"

MainWindow::MainWindow()
{
  m_scanner = new MemScanWidget(this);
  connect(m_scanner,
          static_cast<void (MemScanWidget::*)(u32 address, Common::MemType type, size_t length,
                                              bool isUnsigned, Common::MemBase base)>(
              &MemScanWidget::requestAddWatchEntry),
          this, static_cast<void (MainWindow::*)(u32 address, Common::MemType type, size_t length,
                                                 bool isUnsigned, Common::MemBase base)>(
                    &MainWindow::addWatchRequested));
  m_watcher = new MemWatchWidget(this);

  m_btnAttempHook = new QPushButton("Hook");
  m_btnUnhook = new QPushButton("Unhook");
  connect(m_btnAttempHook, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), this,
          &MainWindow::onHookAttempt);
  connect(m_btnUnhook, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), this,
          &MainWindow::onUnhook);

  QHBoxLayout* dolphinHookButtons_layout = new QHBoxLayout();
  dolphinHookButtons_layout->addWidget(m_btnAttempHook);
  dolphinHookButtons_layout->addWidget(m_btnUnhook);

  m_lblDolphinStatus = new QLabel("");

  m_lblMem2Status = new QLabel("MEM2 is disabled");
  m_btnMem2AutoDetect = new QPushButton("Auto detect");
  m_btnToggleMem2 = new QPushButton("Toggle MEM2");
  connect(m_btnMem2AutoDetect, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked),
          this, &MainWindow::onAutoDetectMem2);
  connect(m_btnToggleMem2, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), this,
          &MainWindow::onToggleMem2);

  QHBoxLayout* mem2Buttons_layout = new QHBoxLayout();
  mem2Buttons_layout->addWidget(m_btnMem2AutoDetect);
  mem2Buttons_layout->addWidget(m_btnToggleMem2);

  QVBoxLayout* mem2Status_layout = new QVBoxLayout();
  mem2Status_layout->addWidget(m_lblMem2Status);
  mem2Status_layout->addLayout(mem2Buttons_layout);

  m_mem2StatusWidget = new QWidget();
  m_mem2StatusWidget->setLayout(mem2Status_layout);

  m_btnOpenMemViewer = new QPushButton("Open memory viewer");
  connect(m_btnOpenMemViewer, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), this,
          &MainWindow::onOpenMenViewer);

  QVBoxLayout* main_layout = new QVBoxLayout;
  main_layout->addWidget(m_lblDolphinStatus);
  main_layout->addLayout(dolphinHookButtons_layout);
  main_layout->addWidget(m_mem2StatusWidget);
  main_layout->addWidget(m_scanner);
  main_layout->addWidget(m_btnOpenMemViewer);
  main_layout->addWidget(m_watcher);

  QWidget* main_widget = new QWidget(this);
  main_widget->setLayout(main_layout);
  setCentralWidget(main_widget);

  m_actOpenWatchList = new QAction("&Open...", this);
  m_actSaveWatchList = new QAction("&Save", this);
  m_actSaveAsWatchList = new QAction("&Save as...", this);

  m_actQuit = new QAction("&Quit", this);
  m_actAbout = new QAction("&About", this);
  connect(m_actOpenWatchList, &QAction::triggered, this, &MainWindow::onOpenWatchFile);
  connect(m_actSaveWatchList, &QAction::triggered, this, &MainWindow::onSaveWatchFile);
  connect(m_actSaveAsWatchList, &QAction::triggered, this, &MainWindow::onSaveAsWatchFile);
  connect(m_actQuit, &QAction::triggered, this, &MainWindow::onQuit);
  connect(m_actAbout, &QAction::triggered, this, &MainWindow::onAbout);

  m_menuFile = menuBar()->addMenu("&File");
  m_menuFile->addAction(m_actOpenWatchList);
  m_menuFile->addAction(m_actSaveWatchList);
  m_menuFile->addAction(m_actSaveAsWatchList);
  m_menuFile->addAction(m_actQuit);
  m_menuHelp = menuBar()->addMenu("&Help");
  m_menuHelp->addAction(m_actAbout);

  connect(m_scanner, &MemScanWidget::mustUnhook, this, &MainWindow::onUnhook);
  connect(m_watcher, &MemWatchWidget::mustUnhook, this, &MainWindow::onUnhook);

  DolphinComm::DolphinAccessor::init();

  m_viewer = new MemViewerWidget(this, Common::MEM1_START);
  connect(m_viewer, &MemViewerWidget::mustUnhook, this, &MainWindow::onUnhook);
  connect(m_watcher,
          static_cast<void (MemWatchWidget::*)(u32)>(&MemWatchWidget::goToAddressInViewer), this,
          static_cast<void (MainWindow::*)(u32)>(&MainWindow::onOpenMemViewerWithAddress));
  m_dlgViewer = new QDialog(this);
  QVBoxLayout* layout = new QVBoxLayout;
  layout->addWidget(m_viewer);
  m_dlgViewer->setLayout(layout);

  // First attempt to hook
  onHookAttempt();
  if (DolphinComm::DolphinAccessor::getStatus() ==
      DolphinComm::DolphinAccessor::DolphinStatus::hooked)
  {
    DolphinComm::DolphinAccessor::autoDetectMem2();
    updateMem2Status();
  }
}

void MainWindow::addWatchRequested(u32 address, Common::MemType type, size_t length,
                                   bool isUnsigned, Common::MemBase base)
{
  MemWatchEntry* newEntry = new MemWatchEntry("No label", address, type, base, isUnsigned, length);
  m_watcher->addWatchEntry(newEntry);
}

void MainWindow::onAutoDetectMem2()
{
  DolphinComm::DolphinAccessor::autoDetectMem2();
  updateDolphinHookingStatus();
  if (DolphinComm::DolphinAccessor::getStatus() ==
      DolphinComm::DolphinAccessor::DolphinStatus::hooked)
    updateMem2Status();
  else
    onUnhook();
}

void MainWindow::onToggleMem2()
{
  if (DolphinComm::DolphinAccessor::isMem2Enabled())
    DolphinComm::DolphinAccessor::enableMem2(false);
  else
    DolphinComm::DolphinAccessor::enableMem2(true);
  updateMem2Status();
}

void MainWindow::onOpenMenViewer()
{
  m_dlgViewer->show();
}

void MainWindow::onOpenMemViewerWithAddress(u32 address)
{
  m_viewer->goToAddress(address);
  m_dlgViewer->show();
}

void MainWindow::updateMem2Status()
{
  if (DolphinComm::DolphinAccessor::isMem2Enabled())
    m_lblMem2Status->setText("MEM2 is enabled");
  else
    m_lblMem2Status->setText("MEM2 is disabled");
}

void MainWindow::updateDolphinHookingStatus()
{
  switch (DolphinComm::DolphinAccessor::getStatus())
  {
  case DolphinComm::DolphinAccessor::DolphinStatus::hooked:
  {
    m_lblDolphinStatus->setText(
        "Hooked successfully to Dolphin, current start address: " +
        QString::number(DolphinComm::DolphinAccessor::getEmuRAMAddressStart(), 16).toUpper());
    m_scanner->setEnabled(true);
    m_watcher->setEnabled(true);
    m_btnOpenMemViewer->setEnabled(true);
    m_btnAttempHook->hide();
    m_btnUnhook->show();
    break;
  }
  case DolphinComm::DolphinAccessor::DolphinStatus::notRunning:
  {
    m_lblDolphinStatus->setText("Cannot hook to Dolphin, the process is not running");
    m_scanner->setDisabled(true);
    m_watcher->setDisabled(true);
    m_btnOpenMemViewer->setDisabled(true);
    m_btnAttempHook->show();
    m_btnUnhook->hide();
    break;
  }
  case DolphinComm::DolphinAccessor::DolphinStatus::noEmu:
  {
    m_lblDolphinStatus->setText(
        "Cannot hook to Dolphin, the process is running, but no emulation has been started");
    m_scanner->setDisabled(true);
    m_watcher->setDisabled(true);
    m_btnOpenMemViewer->setDisabled(true);
    m_btnAttempHook->show();
    m_btnUnhook->hide();
    break;
  }
  case DolphinComm::DolphinAccessor::DolphinStatus::unHooked:
  {
    m_lblDolphinStatus->setText("Unhooked, press \"Hook\" to hook to Dolphin again");
    m_scanner->setDisabled(true);
    m_watcher->setDisabled(true);
    m_btnOpenMemViewer->setDisabled(true);
    m_btnAttempHook->show();
    m_btnUnhook->hide();
    break;
  }
  }
}

void MainWindow::onHookAttempt()
{
  DolphinComm::DolphinAccessor::hook();
  updateDolphinHookingStatus();
  if (DolphinComm::DolphinAccessor::getStatus() ==
      DolphinComm::DolphinAccessor::DolphinStatus::hooked)
  {
    m_scanner->getUpdateTimer()->start(100);
    m_watcher->getUpdateTimer()->start(10);
    m_watcher->getFreezeTimer()->start(10);
    m_viewer->getUpdateTimer()->start(100);
    m_viewer->hookStatusChanged(true);
    m_mem2StatusWidget->setEnabled(true);
  }
  else
  {
    m_mem2StatusWidget->setDisabled(true);
  }
}

void MainWindow::onUnhook()
{
  m_scanner->getUpdateTimer()->stop();
  m_watcher->getUpdateTimer()->stop();
  m_watcher->getFreezeTimer()->stop();
  m_viewer->getUpdateTimer()->stop();
  m_viewer->hookStatusChanged(false);
  m_mem2StatusWidget->setDisabled(true);
  DolphinComm::DolphinAccessor::unHook();
  updateDolphinHookingStatus();
}

void MainWindow::onOpenWatchFile()
{
  if (m_watcher->warnIfUnsavedChanges())
    m_watcher->openWatchFile();
}

void MainWindow::onSaveWatchFile()
{
  m_watcher->saveWatchFile();
}

void MainWindow::onSaveAsWatchFile()
{
  m_watcher->saveAsWatchFile();
}

void MainWindow::onAbout()
{
  QMessageBox::about(this, "About Dolphin memory engine",
                     "Beta version 0.2\n\nA RAM search made to facilitate research and "
                     "reverse engineering of GameCube and Wii games using the Dolphin "
                     "emulator.\n\nThis program is licensed under the MIT license. You "
                     "should have received a copy of the MIT license along with this program");
}

void MainWindow::onQuit()
{
  close();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
  if (m_watcher->warnIfUnsavedChanges())
  {
    event->accept();
  }
  else
  {
    event->ignore();
  }
}