#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "codeeditor.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFontDialog>
#include <QColorDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QTreeView>
#include <QVBoxLayout>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    manager = new QNetworkAccessManager(this);
    // -------------------- 信号槽连接 --------------------
    connect(ui->actionNew, &QAction::triggered, this, &MainWindow::newFileInProject);
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::openFile);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::saveFile);
    connect(ui->actionSave_As, &QAction::triggered, this, &MainWindow::saveFileAs);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::exitApp);
    connect(ui->actionFont, &QAction::triggered, this, &MainWindow::setFont);
    connect(ui->actionColor, &QAction::triggered, this, &MainWindow::setColor);
    connect(ui->actionFindText, &QAction::triggered, this, &MainWindow::findText);
    connect(ui->actionFindNext, &QAction::triggered, this, &MainWindow::findNext);
    connect(ui->actionFindPrevious, &QAction::triggered, this, &MainWindow::findPrevious);
    connect(ui->actionCompile, &QAction::triggered, this, &MainWindow::compileCurrentFile);
    connect(ui->actionRun, &QAction::triggered, this, &MainWindow::runCurrentFile);
    connect(ui->actionAIImprove, &QAction::triggered, this, &MainWindow::aiImproveCode);
    connect(ui->aiChatInput, &QPlainTextEdit::textChanged, this, &MainWindow::checkEnterPressed);
    connect(ui->btnClearHistory, &QPushButton::clicked,
            this, &MainWindow::clearConversationHistory);


    connect(ui->actionOpenProject, &QAction::triggered, this, [=]() {
        chooseProjectDirectory("");
    });
    connect(ui->actionNewProject, &QAction::triggered, this, &MainWindow::createProject);

    connect(ui->tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [=](int index) {
        QWidget* tab = ui->tabWidget->widget(index);
        if (tabFilePaths.contains(tab))
            currentFilePath = tabFilePaths.value(tab);
        else
            currentFilePath = "";
    });

    ui->tabWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tabWidget, &QTabWidget::customContextMenuRequested,
            this, &MainWindow::showTabContextMenu);


    // -------------------- 输出窗口 --------------------
    // 测试输出
    ui->outputWindow->appendPlainText("编译输出窗口初始化完成！");


    // -------------------- 项目树 --------------------
    ui->projectTree->setModel(nullptr);
    projectModel = nullptr;
    currentProjectPath = "";
    // 项目树样式




    statusBar()->showMessage("Ready");
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::aiImproveCode()
{
    // ====== 调试 SSL 环境 ======
    qDebug() << "Supports SSL:" << QSslSocket::supportsSsl();
    qDebug() << "Build version:" << QSslSocket::sslLibraryBuildVersionString();
    qDebug() << "Runtime version:" << QSslSocket::sslLibraryVersionString();

    CodeEditor* editor = currentEditor();
    if (!editor) {
        QMessageBox::warning(this, "AI 改代码", "没有打开的文件！");
        return;
    }

    QString code = editor->toPlainText();

    // ====== 构造请求 ======
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);

    // DeepSeek API 地址
    QNetworkRequest request(QUrl("https://api.deepseek.com/v1/chat/completions"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 使用新的 DeepSeek API Key
    request.setRawHeader("Authorization", "Bearer sk-73a86e4b0df34016b4647887af44ef19");

    QJsonObject body;
    body["model"] = "deepseek-chat";   // DeepSeek 模型名
    QJsonArray messages;
    messages.append(QJsonObject{
        {"role", "system"},
        {"content", "你是一个资深的C/C++开发助手，帮我改进下面的代码，并保持可编译。"}
    });
    messages.append(QJsonObject{
        {"role", "user"},
        {"content", code}
    });
    body["messages"] = messages;

    QNetworkReply* reply = manager->post(request, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, this, [=]() {
        QByteArray response = reply->readAll();
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::warning(this, "AI 改代码",
                                 "请求失败: " + reply->errorString() + "\n" + response);
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(response);
        if (!doc.isObject() || !doc.object().contains("choices")) {
            QMessageBox::warning(this, "AI 改代码", "返回结果无效！\n" + response);
            return;
        }

        QJsonArray choices = doc["choices"].toArray();
        if (choices.isEmpty()) {
            QMessageBox::warning(this, "AI 改代码", "没有生成结果！");
            return;
        }

        QString newCode = choices[0].toObject()
                              .value("message").toObject()
                              .value("content").toString();

        // ====== 打开新Tab显示 AI 改进的代码 ======
        QWidget *tabContainer = new QWidget;
        QHBoxLayout *layout = new QHBoxLayout(tabContainer);
        layout->setSpacing(6);
        layout->setContentsMargins(13, 13, 13, 13);

        CodeEditor *newEditor = createEditor(tabContainer);
        newEditor->setPlainText(newCode);
        layout->addWidget(newEditor);

        int tabIndex = ui->tabWidget->addTab(tabContainer, "AI_Improved.cpp");
        ui->tabWidget->setCurrentIndex(tabIndex);
    });
}

void MainWindow::checkEnterPressed() {
    QString text = ui->aiChatInput->toPlainText();

    // 如果最后一行以回车结束，触发发送
    if(text.endsWith('\n')) {
        text = text.trimmed();  // 去掉多余回车
        if(!text.isEmpty()) {
            sendToAI(text);
        }
        ui->aiChatInput->clear();
    }
}

void MainWindow::sendToAI(const QString &userText)
{
    // ---------- 显示用户输入 ----------
    QTextCharFormat userFormat;
    userFormat.setFontFamily("SimSun");
    userFormat.setFontPointSize(16);
    userFormat.setForeground(Qt::black);

    QTextCursor cursor(ui->aiChatOutput->textCursor());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText("你: " + userText + "\n", userFormat);
    ui->aiChatOutput->setTextCursor(cursor);

    // ---------- 构建请求 ----------
    QNetworkRequest request(QUrl("https://api.deepseek.com/v1/chat/completions"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");


    QString apiKey = "sk-3290f32686b7419f8422491021d4c317";
    request.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());

    QJsonObject body;
    // ✅ 使用 DeepSeek Reasoner 模型
    body["model"] = "deepseek-reasoner";

    // 构建多轮对话历史
    QJsonArray messages = conversationHistory;
    messages.append(QJsonObject{{"role", "user"}, {"content", userText}});
    body["messages"] = messages;

    // ✅ 可选：设置温度参数（控制创造性，0-2之间）
    body["temperature"] = 0.7;

    // ✅ 可选：设置最大令牌数（控制回复长度）
    body["max_tokens"] = 2000;

    QNetworkReply* reply = manager->post(request, QJsonDocument(body).toJson());

    // ---------- 处理 AI 回复 ----------
    connect(reply, &QNetworkReply::finished, this, [=]() {
        QByteArray response = reply->readAll();
        reply->deleteLater();

        QTextCharFormat aiFormat;
        aiFormat.setFontFamily("JetBrains Mono");
        aiFormat.setFontPointSize(16);
        aiFormat.setForeground(Qt::blue);

        QTextCursor cursor(ui->aiChatOutput->textCursor());
        cursor.movePosition(QTextCursor::End);

        if (reply->error() != QNetworkReply::NoError) {
            cursor.insertText("AI: 网络错误或请求失败\n" + reply->errorString() + "\n", aiFormat);
            cursor.insertText("响应内容: " + response + "\n", aiFormat);
            ui->aiChatOutput->setTextCursor(cursor);
            return;
        }

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(response, &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            cursor.insertText("AI: JSON 解析错误: " + parseError.errorString() + "\n", aiFormat);
            cursor.insertText("原始响应: " + response + "\n", aiFormat);
            ui->aiChatOutput->setTextCursor(cursor);
            return;
        }

        if (doc.isObject()) {
            // 检查是否有错误信息
            if (doc.object().contains("error")) {
                QString errorMsg = doc.object()["error"].toObject()["message"].toString();
                cursor.insertText("AI: API错误: " + errorMsg + "\n", aiFormat);
                ui->aiChatOutput->setTextCursor(cursor);
                return;
            }

            if (doc.object().contains("choices")) {
                QJsonArray choices = doc.object()["choices"].toArray();
                if (!choices.isEmpty()) {
                    QString aiReply = choices[0].toObject()
                                          .value("message").toObject()
                                          .value("content").toString();
                    cursor.insertText("AI: " + aiReply + "\n", aiFormat);
                    ui->aiChatOutput->setTextCursor(cursor);

                    // 更新对话历史
                    conversationHistory = messages;
                    conversationHistory.append(QJsonObject{
                        {"role", "assistant"},
                        {"content", aiReply}
                    });
                    return;
                }
            }
        }

        cursor.insertText("AI: 返回结果解析失败\n", aiFormat);
        cursor.insertText("原始响应: " + response + "\n", aiFormat);
        ui->aiChatOutput->setTextCursor(cursor);
    });
}

void MainWindow::clearConversationHistory()
{
    conversationHistory = QJsonArray();  // 清空多轮对话
    ui->aiChatOutput->clear();           // 可选：清空显示区域
}

void MainWindow::newFile()
{
    QStringList types = { "C Source File (*.c)", "C++ Source File (*.cpp)", "Header File (*.h)", "Text File (*.txt)" };
    bool ok;
    QString selectedType = QInputDialog::getItem(this, "新建文件", "选择文件类型:", types, 0, false, &ok);
    if (!ok || selectedType.isEmpty()) return;

    QString ext;
    if (selectedType.contains("*.c")) ext = ".c";
    else if (selectedType.contains("*.cpp")) ext = ".cpp";
    else if (selectedType.contains("*.h")) ext = ".h";
    else ext = ".txt";

    QString title = "Untitled" + ext;

    QWidget *tabContainer = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(tabContainer);
    layout->setSpacing(6);
    layout->setContentsMargins(13, 13, 13, 13);

    CodeEditor *editor = createEditor(tabContainer);
    layout->addWidget(editor);

    int tabIndex = ui->tabWidget->addTab(tabContainer, title);
    ui->tabWidget->setCurrentIndex(tabIndex);
    editor->setFocus();

    tabFilePaths[tabContainer] = "";
    tabSavedContent[tabContainer] = "";
}

void MainWindow::newFileInProject()
{
    if (currentProjectPath.isEmpty()) {
        newFile();
        return;
    }

    QStringList types = { "C Source File (*.c)", "C++ Source File (*.cpp)", "Header File (*.h)", "Text File (*.txt)" };
    bool ok;
    QString selectedType = QInputDialog::getItem(this, "新建文件", "选择文件类型:", types, 0, false, &ok);
    if (!ok || selectedType.isEmpty()) return;

    QString ext;
    if (selectedType.contains("*.c")) ext = ".c";
    else if (selectedType.contains("*.cpp")) ext = ".cpp";
    else if (selectedType.contains("*.h")) ext = ".h";
    else ext = ".txt";

    bool created = false;
    QString filename;
    int counter = 1;

    while (!created) {
        filename = QString("%1/NewFile%2%3").arg(currentProjectPath).arg(counter).arg(ext);
        if (!QFile::exists(filename)) {
            QFile file(filename);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out.setCodec("UTF-8");
                out << "";
                file.close();
                created = true;
            }
        }
        counter++;
    }

    openFileRoutine(filename);

    if (projectModel) {
        projectModel->setRootPath(currentProjectPath);
        ui->projectTree->setRootIndex(projectModel->index(currentProjectPath));
    }

    statusBar()->showMessage("新建文件: " + QFileInfo(filename).fileName(), 2000);
}

void MainWindow::openFile()
{
    QString filename = QFileDialog::getOpenFileName(this, "Open File", "", "C/C++/Text Files (*.c *.cpp *.h *.txt)");
    if (filename.isEmpty()) return;
    filename = QDir::toNativeSeparators(filename);

    for (auto it = tabFilePaths.constBegin(); it != tabFilePaths.constEnd(); ++it) {
        if (QDir::toNativeSeparators(it.value()) == filename) {
            int index = ui->tabWidget->indexOf(it.key());
            if (index != -1) ui->tabWidget->setCurrentIndex(index);
            QMessageBox::information(this, "提示", "该文件已打开！");
            return;
        }
    }

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ui->outputWindow->appendPlainText("无法打开文件: " + filename);
        return;
    }

    QTextStream in(&file);
    in.setCodec("UTF-8");
    QString content = in.readAll();
    file.close();

    QWidget *tabContainer = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(tabContainer);
    layout->setSpacing(6);
    layout->setContentsMargins(13, 13, 13, 13);

    CodeEditor *editor = createEditor(tabContainer);
    editor->setPlainText(content);
    layout->addWidget(editor);

    int tabIndex = ui->tabWidget->addTab(tabContainer, QFileInfo(filename).fileName());
    ui->tabWidget->setCurrentIndex(tabIndex);

    tabFilePaths[tabContainer] = filename;
    tabSavedContent[tabContainer] = content;

    statusBar()->showMessage("Opened: " + filename, 2000);
}

void MainWindow::openFileRoutine(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Open File", "Cannot open file: " + file.errorString());
        return;
    }

    QTextStream in(&file);
    in.setCodec("UTF-8");
    QString content = in.readAll();
    file.close();

    QWidget *tabContainer = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(tabContainer);
    layout->setSpacing(6);
    layout->setContentsMargins(13, 13, 13, 13);

    CodeEditor *editor = createEditor(tabContainer);
    editor->setPlainText(content);
    layout->addWidget(editor);

    int tabIndex = ui->tabWidget->addTab(tabContainer, QFileInfo(filePath).fileName());
    ui->tabWidget->setCurrentIndex(tabIndex);

    tabFilePaths[tabContainer] = filePath;
    tabSavedContent[tabContainer] = content;
}

void MainWindow::saveFile()
{
    QWidget *tab = ui->tabWidget->currentWidget();
    if (!tab) return;

    QString filePath;
    if (tabFilePaths.contains(tab)) {
        filePath = tabFilePaths.value(tab);
    } else if (CodeEditor *editorTab = qobject_cast<CodeEditor*>(tab)) {
        if (tabFilePaths.contains(editorTab))
            filePath = tabFilePaths.value(editorTab);
        else {
            saveFileAs();
            return;
        }
    } else {
        saveFileAs();
        return;
    }

    CodeEditor *editor = tab->findChild<CodeEditor*>();
    if (!editor) {
        editor = qobject_cast<CodeEditor*>(tab);
    }
    if (!editor) return;

    QString content = editor->toPlainText();
    if (filePath.isEmpty()) {
        saveFileAs();
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "保存失败", "无法保存文件：" + filePath);
        return;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << content;
    file.close();

    tabSavedContent[tab] = content;
    statusBar()->showMessage("已保存: " + QFileInfo(filePath).fileName(), 2000);
}

void MainWindow::saveFileAs()
{
    QWidget *tab = ui->tabWidget->currentWidget();
    if (!tab) return;

    CodeEditor *editor = tab->findChild<CodeEditor*>();
    if (!editor) return;

    QString filename = QFileDialog::getSaveFileName(
        this,
        "Save As",
        "",
        "C/C++/Text Files (*.c *.cpp *.h *.txt)"
    );
    if (filename.isEmpty()) return;

    filename = QDir::toNativeSeparators(filename);

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法保存文件: " + filename);
        return;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << editor->toPlainText();
    file.close();

    tabFilePaths[tab] = filename;
    tabSavedContent[tab] = editor->toPlainText();

    int index = ui->tabWidget->indexOf(tab);
    if (index != -1) ui->tabWidget->setTabText(index, QFileInfo(filename).fileName());

    statusBar()->showMessage("另存为成功: " + filename, 2000);
}

void MainWindow::chooseProjectDirectory(const QString &defaultPath)
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择工程目录", defaultPath);
    if (dir.isEmpty()) return;

    // -------------------- 检查未保存文件 --------------------
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        QWidget* tab = ui->tabWidget->widget(i);
        CodeEditor* editor = qobject_cast<CodeEditor*>(tab);
        if (!editor) editor = tab->findChild<CodeEditor*>();
        if (!editor) continue;

        QString savedContent = tabSavedContent.value(tab, "");
        if (editor->toPlainText() != savedContent) {
            ui->tabWidget->setCurrentWidget(tab);
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "未保存的更改",
                QString("文件 %1 有未保存的更改，是否保存？").arg(ui->tabWidget->tabText(i)),
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
            );

            if (reply == QMessageBox::Yes) {
                saveFile();
            } else if (reply == QMessageBox::Cancel) {
                return;
            }
        }
    }

    // -------------------- 清理旧项目 --------------------
    while (ui->tabWidget->count() > 0) {
        closeTab(0);
    }
    tabFilePaths.clear();
    tabSavedContent.clear();

    if (projectModel) {
        projectModel->deleteLater();
        projectModel = nullptr;
    }

    currentProjectPath = dir;

    // -------------------- 加载新项目 --------------------
    projectModel = new QFileSystemModel(this);
    projectModel->setRootPath(dir);
    projectModel->setNameFilters(QStringList() << "*.cpp" << "*.c" << "*.h");
    projectModel->setNameFilterDisables(false);

    ui->projectTree->setModel(projectModel);
    ui->projectTree->setRootIndex(projectModel->index(dir));

    ui->projectTree->disconnect();

    connect(ui->projectTree, &QTreeView::doubleClicked, this, [=](const QModelIndex &index) {
        QString path = projectModel->filePath(index);
        QFileInfo info(path);
        if (info.isFile()) openFileRoutine(path);
    });

    // -------------------- 显示项目名称和路径 --------------------
    QString projectName = QFileInfo(currentProjectPath).fileName();
    setWindowTitle(QString("CIDE - %1 [%2]").arg(projectName).arg(currentProjectPath));

    statusBar()->showMessage("已切换到项目: " + currentProjectPath, 2000);
}

void MainWindow::createProject()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择新项目保存目录");
    if (dir.isEmpty()) return;

    bool ok;
    QString projectName = QInputDialog::getText(this, "新建项目", "项目名称:", QLineEdit::Normal, "", &ok);
    if (!ok || projectName.isEmpty()) return;

    QDir projectDir(dir);
    QString projectPath = projectDir.filePath(projectName);

    if (QDir(projectPath).exists()) {
        QMessageBox::warning(this, "错误", "项目已存在！");
        return;
    }

    if (!projectDir.mkdir(projectName)) {
        QMessageBox::warning(this, "错误", "创建项目失败");
        return;
    }

    // ---------- 设置当前项目路径 ----------
    currentProjectPath = projectPath;

    // ---------- 创建 main.cpp ----------
    QString mainFilePath = currentProjectPath + "/main.cpp";
    QFile file(mainFilePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out.setCodec("UTF-8");
        out << "#include <iostream>\n\nint main() {\n    std::cout << \"Hello World!\" << std::endl;\n    return 0;\n}\n";
        file.close();
    }

    // ---------- 手动加载新项目，不弹出目录选择框 ----------
    QString dirToLoad = currentProjectPath;

    // 清理旧项目
    while (ui->tabWidget->count() > 0) closeTab(0);
    tabFilePaths.clear();
    tabSavedContent.clear();

    if (projectModel) {
        projectModel->deleteLater();
        projectModel = nullptr;
    }

    // 加载新项目
    projectModel = new QFileSystemModel(this);
    projectModel->setRootPath(dirToLoad);
    projectModel->setNameFilters(QStringList() << "*.cpp" << "*.c" << "*.h");
    projectModel->setNameFilterDisables(false);

    ui->projectTree->setModel(projectModel);
    ui->projectTree->setRootIndex(projectModel->index(dirToLoad));

    ui->projectTree->disconnect();
    connect(ui->projectTree, &QTreeView::doubleClicked, this, [=](const QModelIndex &index) {
        QString path = projectModel->filePath(index);
        QFileInfo info(path);
        if (info.isFile()) openFileRoutine(path);
    });

    // 显示项目名称和路径
    QString displayName = QFileInfo(currentProjectPath).fileName();
    setWindowTitle(QString("CIDE - %1 [%2]").arg(displayName).arg(currentProjectPath));
    statusBar()->showMessage("已切换到项目: " + currentProjectPath, 2000);

    // 打开 main.cpp
    openFileRoutine(mainFilePath);
}

CodeEditor* MainWindow::currentEditor()
{
    QWidget *tab = ui->tabWidget->currentWidget();
    if (!tab) return nullptr;

    CodeEditor *editor = qobject_cast<CodeEditor*>(tab);
    if (editor) return editor;

    return tab->findChild<CodeEditor*>();
}

QStringList MainWindow::collectSourceFiles(const QString &dirPath)
{
    QStringList files;
    QDir dir(dirPath);
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &info : entries) {
        if (info.isDir()) {
            files.append(collectSourceFiles(info.absoluteFilePath()));
        } else if (info.suffix() == "cpp" || info.suffix() == "c") {
            files.append(info.absoluteFilePath());
        }
    }
    return files;
}

CodeEditor* MainWindow::createEditor(QWidget *parent)
{
    CodeEditor *editor = new CodeEditor(parent);
    QFont font("Consolas", 14);
    editor->setFont(font);
    return editor;
}

void MainWindow::setFont()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, this);
    if (!ok) return;

    CodeEditor *editor = currentEditor();
    if (!editor) return;

    editor->setFont(font);
    editor->update();
}

void MainWindow::setColor()
{
    QColor color = QColorDialog::getColor(Qt::black, this);
    if (!color.isValid()) return;

    CodeEditor *editor = currentEditor();
    if (!editor) return;

    QPalette pal = editor->palette();
    pal.setColor(QPalette::Text, color);
    editor->setPalette(pal);
}

void MainWindow::exitApp()
{
    QApplication::quit();
}

void MainWindow::findText()
{
    CodeEditor *editor = currentEditor();
    if (!editor) return;

    bool ok;
    QString search = QInputDialog::getText(this, "Find", "Enter text to find:", QLineEdit::Normal, lastSearchText, &ok);
    if (!ok || search.isEmpty()) return;

    lastSearchText = search;
    QString content = editor->toPlainText();
    QTextCursor cursor(editor->document());

    QTextCharFormat clearFormat;
    clearFormat.setBackground(Qt::transparent);
    cursor.select(QTextCursor::Document);
    cursor.setCharFormat(clearFormat);

    QTextCharFormat format;
    format.setBackground(Qt::yellow);

    searchResults.clear();
    int pos = 0;
    while ((pos = content.indexOf(search, pos, Qt::CaseSensitive)) != -1) {
        QTextCursor highlightCursor(editor->document());
        highlightCursor.setPosition(pos);
        highlightCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, search.length());
        highlightCursor.setCharFormat(format);
        searchResults.append(highlightCursor);
        pos += search.length();
    }

    if (searchResults.isEmpty()) {
        QMessageBox::information(this, "Find", "Text not found.");
        currentResultIndex = -1;
    } else {
        currentResultIndex = 0;
        editor->setTextCursor(searchResults[currentResultIndex]);
        editor->setFocus();
    }
}

void MainWindow::findNext()
{
    CodeEditor *editor = currentEditor();
    if (!editor || searchResults.isEmpty()) return;

    currentResultIndex = (currentResultIndex + 1) % searchResults.size();
    editor->setTextCursor(searchResults[currentResultIndex]);
    editor->setFocus();
}

void MainWindow::findPrevious()
{
    CodeEditor *editor = currentEditor();
    if (!editor || searchResults.isEmpty()) return;

    currentResultIndex = (currentResultIndex - 1 + searchResults.size()) % searchResults.size();
    editor->setTextCursor(searchResults[currentResultIndex]);
    editor->setFocus();
}

void MainWindow::closeTab(int index)
{
    QWidget *tab = ui->tabWidget->widget(index);
    if (!tab) return;

    CodeEditor *editor = qobject_cast<CodeEditor*>(tab);
    if (!editor) editor = tab->findChild<CodeEditor*>();
    if (!editor) {
        ui->tabWidget->removeTab(index);
        tabFilePaths.remove(tab);
        tabSavedContent.remove(tab);
        tab->deleteLater();
        return;
    }

    if (!editor->document()->isModified() ||
        tabSavedContent.value(tab, QString()) == editor->toPlainText()) {
        ui->tabWidget->removeTab(index);
        tabFilePaths.remove(tab);
        tabSavedContent.remove(tab);
        tab->deleteLater();
        return;
    }

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "未保存的更改",
                                  "此文档有未保存的更改，是否保存？",
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if (reply == QMessageBox::Yes) {
        ui->tabWidget->setCurrentWidget(tab);
        saveFile();
        if (!editor->document()->isModified()) {
            ui->tabWidget->removeTab(index);
            tabFilePaths.remove(tab);
            tabSavedContent.remove(tab);
            tab->deleteLater();
        }
    } else if (reply == QMessageBox::No) {
        ui->tabWidget->removeTab(index);
        tabFilePaths.remove(tab);
        tabSavedContent.remove(tab);
        tab->deleteLater();
    }
}

void MainWindow::compileCurrentFile()
{
    saveFile();

    QStringList filesToCompile;

    if (!currentProjectPath.isEmpty()) {
        filesToCompile = collectSourceFiles(currentProjectPath);
        if (filesToCompile.isEmpty()) {
            QMessageBox::warning(this, "提示", "项目中没有源文件！");
            return;
        }
    } else {
        QWidget* tab = ui->tabWidget->currentWidget();
        if (!tab) {
            QMessageBox::warning(this, "提示", "没有可编译的文件！");
            return;
        }

        QString filePath = tabFilePaths.value(tab);
        if (filePath.isEmpty()) {
            QMessageBox::warning(this, "提示", "请先保存文件后再编译！");
            return;
        }

        filesToCompile << filePath;
    }

    QString appDir = QCoreApplication::applicationDirPath();
    QString exePath = QDir(appDir).filePath("temp.exe");
    QString gppPath = QDir(appDir).filePath("mingw/bin/g++.exe");

    ui->outputWindow->clear();
    ui->outputWindow->appendPlainText("🔨 正在编译...");

    if (QFile::exists(exePath)) QFile::remove(exePath);

    QProcess compileProcess;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PATH", env.value("PATH") + ";" + QDir(appDir).filePath("mingw/bin"));
    compileProcess.setProcessEnvironment(env);

    QStringList args;
    for (const QString &f : filesToCompile)
        args << QDir::toNativeSeparators(f);

    args << "-o" << QDir::toNativeSeparators(exePath);

    qint64 startTime = QDateTime::currentMSecsSinceEpoch();
    compileProcess.start(gppPath, args);
    compileProcess.waitForFinished();
    qint64 endTime = QDateTime::currentMSecsSinceEpoch();

    QString output = compileProcess.readAllStandardOutput();
    QString errors = compileProcess.readAllStandardError();

    if (!output.isEmpty()) ui->outputWindow->appendPlainText(output.trimmed());
    if (!errors.isEmpty()) ui->outputWindow->appendPlainText(errors.trimmed());

    double elapsedSec = (endTime - startTime) / 1000.0;

    if (compileProcess.exitCode() != 0)
        ui->outputWindow->appendPlainText("❌ 编译失败！");
    else
        ui->outputWindow->appendPlainText(
            QString("✅ 编译成功，生成：%1 （耗时 %2 秒）")
            .arg(exePath)
            .arg(elapsedSec, 0, 'f', 2)
        );

    ui->outputWindow->appendPlainText("=== Compile Finished ===");
}

void MainWindow::runCurrentFile()
{
    saveFile();

    QString appDir = QCoreApplication::applicationDirPath();
    QString exePath = QDir(appDir).filePath("temp.exe");

    if (!QFile::exists(exePath)) {
        compileCurrentFile();
        if (!QFile::exists(exePath)) return;
    }

#ifdef Q_OS_WIN
    QStringList runArgs;
    runArgs << "/C" << "start" << "cmd" << "/K"
            << "chcp 65001 > nul && " + exePath;

    if (!QProcess::startDetached("cmd.exe", runArgs)) {
        ui->outputWindow->appendPlainText("❌ 无法启动程序！");
    }
#else
    if (!QProcess::startDetached(exePath, QStringList(), QFileInfo(exePath).absolutePath())) {
        ui->outputWindow->appendPlainText("❌ 无法启动程序！");
    }
#endif
}

void MainWindow::showTabContextMenu(const QPoint &pos)
{
    int index = ui->tabWidget->tabBar()->tabAt(pos);
    if (index == -1) return;

    QMenu menu;
    QAction *renameAction = menu.addAction("重命名文件");
    QAction *selectedAction = menu.exec(ui->tabWidget->tabBar()->mapToGlobal(pos));
    if (selectedAction == renameAction) {
        renameTabFile(index);
    }
}

void MainWindow::renameTabFile(int index)
{
    QWidget *tab = ui->tabWidget->widget(index);
    if (!tab) return;

    QString oldPath = tabFilePaths.value(tab, "");
    QString oldName = oldPath.isEmpty() ? ui->tabWidget->tabText(index) : QFileInfo(oldPath).fileName();

    bool ok;
    QString newName = QInputDialog::getText(this, "重命名文件",
                                            "请输入新文件名:",
                                            QLineEdit::Normal,
                                            oldName, &ok);
    if (!ok || newName.isEmpty()) return;

    if (!oldPath.isEmpty()) {
        QFileInfo info(oldPath);
        QString newPath = info.absolutePath() + "/" + newName;

        if (QFile::exists(newPath)) {
            QMessageBox::warning(this, "错误", "文件已存在！");
            return;
        }

        QFile::rename(oldPath, newPath);
        tabFilePaths[tab] = newPath;
    }

    ui->tabWidget->setTabText(index, newName);
    statusBar()->showMessage("重命名成功: " + newName, 2000);
}
