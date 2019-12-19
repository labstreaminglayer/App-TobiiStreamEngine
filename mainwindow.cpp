#pragma warning(push)
#pragma warning(disable : 26812)

#include "mainwindow.h"
#include "ui_mainwindow.h"

//: Qt headers
#include <QCloseEvent>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
//: standard C++ headers
#include <fstream>
#include <lsl_cpp.h>
#include <string>
#include <vector>
#pragma warning( pop )

//: Device headers
#include <tobii/tobii.h>
#include <tobii/tobii_streams.h>


static auto list_devices(tobii_api_t* api)
{
	std::vector<std::string> result;
	auto error = tobii_enumerate_local_device_urls(api,
		[](char const* url, void* user_data) // Use a lambda for url receiver function
		{
			// Add url string to the supplied result vector
			auto list = (std::vector<std::string>*) user_data;
			list->push_back(url);
		}, &result);
	// TODO: Handle error.
	//if (error != TOBII_ERROR_NO_ERROR) std::cerr << "Failed to enumerate devices." << std::endl;

	return result;
}


static auto device_connect(tobii_api_t* api, std::string url, const unsigned int retries, const unsigned int interval, tobii_device_t** device)
{
	// std::cout << "Connecting to " << url << " (trying " << retries << " times with " << interval << " milliseconds intervals)." << std::endl;

	unsigned int retry = 0;
	auto error = TOBII_ERROR_NO_ERROR;
	do
	{
		error = tobii_device_create(api, url.c_str(), TOBII_FIELD_OF_USE_INTERACTIVE, device);
		if ((error != TOBII_ERROR_CONNECTION_FAILED) && (error != TOBII_ERROR_FIRMWARE_UPGRADE_IN_PROGRESS)) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(interval));
		++retry;
	} while (retry < retries);

	if (error != TOBII_ERROR_NO_ERROR)
	{
		// std::cerr << "Failed connecting to " << url;
		if ((error == TOBII_ERROR_CONNECTION_FAILED) || (error == TOBII_ERROR_FIRMWARE_UPGRADE_IN_PROGRESS))
		{
			// std::cerr << " (tried " << retries << " times with " << interval << " milliseconds intervals)";
		}
		// std::cerr << "." << std::endl;
	}

	return error;
}


static auto device_reconnect(tobii_device_t* device, const unsigned int retries, const unsigned int interval)
{
	// std::cout << "Reconnecting (trying " << retries << " times with " << interval << " milliseconds intervals)." << std::endl;

	unsigned int retry = 0;
	auto error = TOBII_ERROR_NO_ERROR;
	do
	{
		error = tobii_device_reconnect(device);
		if ((error != TOBII_ERROR_CONNECTION_FAILED) && (error != TOBII_ERROR_FIRMWARE_UPGRADE_IN_PROGRESS)) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(interval));
		++retry;
	} while (retry < retries);

	if (error != TOBII_ERROR_NO_ERROR)
	{
		// std::cerr << "Failed reconnecting";
		if ((error == TOBII_ERROR_CONNECTION_FAILED) || (error == TOBII_ERROR_FIRMWARE_UPGRADE_IN_PROGRESS))
		{
			// std::cerr << " (tried " << retry << " times with " << interval << " milliseconds intervals)";
		}
		// std::cerr << "." << std::endl;
	}

	return error;
}


MainWindow::MainWindow(QWidget *parent, const char *config_file)
	: QMainWindow(parent), ui(new Ui::MainWindow) {
	ui->setupUi(this);
	connect(ui->actionLoad_Configuration, &QAction::triggered, [this]() {
		load_config(QFileDialog::getOpenFileName(
			this, "Load Configuration File", "", "Configuration Files (*.cfg)"));
	});
	connect(ui->actionSave_Configuration, &QAction::triggered, [this]() {
		save_config(QFileDialog::getSaveFileName(
			this, "Save Configuration File", "", "Configuration Files (*.cfg)"));
	});
	connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::close);
	connect(ui->actionAbout, &QAction::triggered, [this]() {
		QString infostr = QStringLiteral("LSL library version: ") +
						  QString::number(lsl::library_version()) +
						  "\nLSL library info:" + lsl::lsl_library_info();
		QMessageBox::about(this, "About this app", infostr);
	});
	connect(ui->linkButton, &QPushButton::clicked, this, &MainWindow::toggleRecording);
	connect(ui->pushButton_refresh, &QPushButton::clicked, this, &MainWindow::refreshDevices);

	//: At the end of the constructor, we load the supplied config file or find it
	//: in one of the default paths
	QString cfgfilepath = find_config_file(config_file);
	refreshDevices();
	load_config(cfgfilepath);
}


void MainWindow::refreshDevices() {

	tobii_api_t* api;
	auto error = tobii_api_create(&api, nullptr, nullptr);
	if (error != TOBII_ERROR_NO_ERROR)
	{
		//std::cerr << "Failed to initialize the Tobii Stream Engine API." << std::endl;
	}

	std::vector<std::string> devices = list_devices(api);

	// TOOD: populate a dropdown list.
	ui->comboBox_device->clear();
	for each (auto dev_url in devices)
	{
		ui->comboBox_device->addItem(QString::fromStdString(dev_url));
	}

	tobii_api_destroy(api);
}



void MainWindow::load_config(const QString &filename) {
	QSettings settings(filename, QSettings::Format::IniFormat);
	ui->input_name->setText(settings.value("TobiiStreamEngine/name", "TobiiStreamEngine").toString());
}


void MainWindow::save_config(const QString &filename) {
	QSettings settings(filename, QSettings::Format::IniFormat);
	settings.beginGroup("TobiiStreamEngine");
	settings.setValue("name", ui->input_name->text());
	settings.sync();
}


void MainWindow::closeEvent(QCloseEvent *ev) {
	if (reader) {
		QMessageBox::warning(this, "Recording still running", "Can't quit while recording");
		ev->ignore();
	}
}


/*: ## The recording thread
 *
 * We run the recording in a separate thread to keep the UI responsive.
 * The recording thread function generally gets called with
 *
 * - the configuration parameters (here `name`, `device_param`)
 * - a reference to an `std::atomic<bool>`
 *
 * the shutdown flag indicates that the recording should stop as soon as possible */
void recording_thread_function(
	std::string name, std::string device_url, std::atomic<bool> &shutdown) {

	// Preinit some variables
	tobii_api_t* api;
	std::vector<std::string> devices;
	std::string selected_device = "";
	// Try to connect during 30 seconds with an interval of 100 milliseconds
	const unsigned int retries = 300;
	const unsigned int interval = 100;
	tobii_device_t* device = nullptr;
	
	//: create an outlet and a send buffer
	lsl::stream_info info(name, "gaze", 2);
	lsl::stream_outlet outlet(info);

	// Create interface to Tobii API
	auto error = tobii_api_create(&api, nullptr, nullptr);
	if (error != TOBII_ERROR_NO_ERROR)
	{
		//std::cerr << "Failed to initialize the Tobii Stream Engine API." << std::endl;
		goto cleanup_nodevice;
	}

	// Get list of devices.
	devices = list_devices(api);
	if (devices.size() == 0)
	{
		//std::cerr << "No stream engine compatible device(s) found." << std::endl;
		goto cleanup;
	}

	// Find device matching device_url
	if (std::find(devices.begin(), devices.end(), device_url) != devices.end())
	{
		selected_device = device_url;
	}
	else {
		goto cleanup;
	}

	error = device_connect(api, selected_device, retries, interval, &device);
	if (error != TOBII_ERROR_NO_ERROR)
	{
		goto cleanup;
	}

	// Start subscribing to gaze and supply lambda callback function to handle the gaze point data
	error = tobii_gaze_point_subscribe(device,
		[](tobii_gaze_point_t const* gaze_point, void* user_data)
		{
			auto p_outlet = (lsl::stream_outlet*)user_data;  // Cast user_data back to outlet pointer
			if (gaze_point->validity == TOBII_VALIDITY_VALID)
			{
				double timestamp_s = (double)gaze_point->timestamp_us / 1000000;
				std::vector<float> sample = {gaze_point->position_xy[0], gaze_point->position_xy[1] };
				p_outlet->push_sample(sample, timestamp_s);
			}
			else
			{
				//std::cout << "Gaze point: " << gaze_point->timestamp_us << " INVALID" << std::endl;
			}
		}, &outlet);

	while (!shutdown) {
		auto error = tobii_wait_for_callbacks(1, &device);

		if (error == TOBII_ERROR_TIMED_OUT) continue; // If timed out, redo the wait for callbacks call
		else if (error != TOBII_ERROR_NO_ERROR)
		{
			//std::cerr << "tobii_wait_for_callbacks failed: " << tobii_error_message(error) << "." << std::endl;
			break;
		}
		// Calling this function will execute the subscription callback functions
		error = tobii_device_process_callbacks(device);
		if (error == TOBII_ERROR_CONNECTION_FAILED)
		{
			// Block here while attempting reconnect, if it fails, exit the thread
			error = device_reconnect(device, interval, retries);
			if (error != TOBII_ERROR_NO_ERROR)
			{
				//std::cerr << "Connection was lost and reconnection failed." << std::endl;
				break;
			}
			continue;
		}
		else if (error != TOBII_ERROR_NO_ERROR)
		{
			//std::cerr << "tobii_device_process_callbacks failed: " << tobii_error_message(error) << "." << std::endl;
			break;
		}

	}

cleanup:
	error = tobii_gaze_point_unsubscribe(device);
cleanup_nodevice:
	if (error != TOBII_ERROR_NO_ERROR) {}
		//std::cerr << "Failed to unsubscribe from gaze stream." << std::endl;
	error = tobii_device_destroy(device);
	if (error != TOBII_ERROR_NO_ERROR) {}
	//std::cerr << "Failed to destroy device." << std::endl;
	error = tobii_api_destroy(api);
	if (error != TOBII_ERROR_NO_ERROR) {}
	//std::cerr << "Failed to destroy API." << std::endl;
}


//: ## Toggling the recording state
//: Our record button has two functions: start a recording and
//: stop it if a recording is running already.
void MainWindow::toggleRecording() {
	/*: the `std::unique_ptr` evaluates to false if it doesn't point to an object,
	 * so we need to start a recording.
	 * First, we load the configuration from the UI fields, set the shutdown flag
	 * to false so the recording thread doesn't quit immediately and create the
	 * recording thread. */
	if (!reader) {
		// read the configuration from the UI fields
		std::string name = ui->input_name->text().toStdString();

		// TODO: Get device_url from widget.
		std::string device_url = ui->comboBox_device->currentText().toStdString();  // TODO: get from ui->comboBox_device

		shutdown = false;
		/*: `make_unique` allocates a new `std::thread` with our recording
		 * thread function as first parameters and all parameters to the
		 * function after that.
		 * Reference parameters have to be wrapped as a `std::ref`. */
		reader = std::make_unique<std::thread>(
			&recording_thread_function, name, device_url, std::ref(shutdown));
		ui->linkButton->setText("Unlink");
	} else {
		/*: Shutting a thread involves 3 things:
		 * - setting the shutdown flag so the thread doesn't continue acquiring data
		 * - wait for the thread to complete (`join()`)
		 * - delete the thread object and set the variable to nullptr */
		shutdown = true;
		reader->join();
		reader.reset();
		ui->linkButton->setText("Link");
	}
}

/**
 * Find a config file to load. This is (in descending order or preference):
 * - a file supplied on the command line
 * - [executablename].cfg in one the the following folders:
 *	- the current working directory
 *	- the default config folder, e.g. '~/Library/Preferences' on OS X
 *	- the executable folder
 * @param filename	Optional file name supplied e.g. as command line parameter
 * @return Path to a found config file
 */
QString MainWindow::find_config_file(const char *filename) {
	if (filename) {
		QString qfilename(filename);
		if (!QFileInfo::exists(qfilename))
			QMessageBox(QMessageBox::Warning, "Config file not found",
				QStringLiteral("The file '%1' doesn't exist").arg(qfilename), QMessageBox::Ok,
				this);
		else
			return qfilename;
	}
	QFileInfo exeInfo(QCoreApplication::applicationFilePath());
	QString defaultCfgFilename(exeInfo.completeBaseName() + ".cfg");
	QStringList cfgpaths;
	cfgpaths << QDir::currentPath()
			 << QStandardPaths::standardLocations(QStandardPaths::ConfigLocation) << exeInfo.path();
	for (auto path : cfgpaths) {
		QString cfgfilepath = path + QDir::separator() + defaultCfgFilename;
		if (QFileInfo::exists(cfgfilepath)) return cfgfilepath;
	}
	QMessageBox(QMessageBox::Warning, "No config file not found",
		QStringLiteral("No default config file could be found"), QMessageBox::Ok, this);
	return "";
}


//: Tell the compiler to put the default destructor in this object file
MainWindow::~MainWindow() noexcept = default;
