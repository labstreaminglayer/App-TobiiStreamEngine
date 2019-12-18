//: # `mainwindow.h` ([source](../appskeleton/mainwindow.h))
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <atomic>
#include <memory> //for std::unique_ptr
#include <thread>


//: to keep our include lists and compile times short we only provide forward
//: declarations for classes we only have pointers to
namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
	Q_OBJECT
public:
	explicit MainWindow(QWidget *parent, const char *config_file);
	~MainWindow() noexcept override;

private slots:
	void closeEvent(QCloseEvent *ev) override;
	void toggleRecording();

private:
	// function for loading / saving the config file
	QString find_config_file(const char *filename);
	void load_config(const QString &filename);
	void save_config(const QString &filename);
	//: `std::unique_ptr` prevents us from copying objects we should only have
	//: once and automatically deletes the objects when the `unique_ptr` goes
	//: out of scope.
	std::unique_ptr<std::thread> reader{nullptr};

	std::unique_ptr<Ui::MainWindow> ui; // window pointer
	std::atomic<bool> shutdown{false};  // flag indicating whether the recording thread should quit
};

#endif // MAINWINDOW_H
