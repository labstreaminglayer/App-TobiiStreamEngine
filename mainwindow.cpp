#pragma warning(push)
#pragma warning(disable : 26812 26439 26451 26495 26498)

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
#include <string>
#include <vector>
#include <iostream>

#include <lsl_cpp.h>

//: Device headers
#include <tobii/tobii.h>
#include <tobii/tobii_streams.h>

#pragma warning( pop )




static auto list_devices(tobii_api_t* api, tobii_error_t& err)
{
	std::vector<std::string> result;
	err = tobii_enumerate_local_device_urls(api,
		[](char const* url, void* user_data) // Use a lambda for url receiver function
		{
			// Add url string to the supplied result vector
			auto list = (std::vector<std::string>*) user_data;
			list->push_back(url);
		}, &result);

	return result;
}


static auto device_connect(tobii_api_t* api, std::string url, const unsigned int retries, const unsigned int interval, tobii_device_t** device)
{
	std::cout << "Connecting to " << url << " (trying " << retries << " times with " << interval << " milliseconds intervals)." << std::endl;

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
			std::cerr << " (tried " << retries << " times with " << interval << " milliseconds intervals)";
		}
		std::cerr << "." << std::endl;
	}

	return error;
}


static auto device_reconnect(tobii_device_t* device, const unsigned int retries, const unsigned int interval)
{
	std::cout << "Reconnecting (trying " << retries << " times with " << interval << " milliseconds intervals)." << std::endl;

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
		std::cerr << "Failed reconnecting";
		if ((error == TOBII_ERROR_CONNECTION_FAILED) || (error == TOBII_ERROR_FIRMWARE_UPGRADE_IN_PROGRESS))
		{
			std::cerr << " (tried " << retry << " times with " << interval << " milliseconds intervals)";
		}
		std::cerr << "." << std::endl;
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
	tobii_error_t error = tobii_api_create(&api, nullptr, nullptr);
	if (error != TOBII_ERROR_NO_ERROR)
	{
		updateStatus("Failed to initialize the Tobii Stream Engine API.");
	}

	std::vector<std::string> devices = list_devices(api, error);
	if (error != TOBII_ERROR_NO_ERROR) {
		updateStatus("Failed to enumerate devices.");
	}

	// Populate a dropdown list.
	ui->comboBox_device->clear();
	for each (std::string dev_url in devices)
	{
		ui->comboBox_device->addItem(QString::fromStdString(dev_url));
	}

	error = tobii_api_destroy(api);
	if (error != TOBII_ERROR_NO_ERROR) {
		updateStatus("Failed to destroy API after enumerating devices.");
	}
}


void MainWindow::updateStatus(const char* message)
{
	this->statusBar()->showMessage(QString(message));
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
	int64_t last_sync_time;
	
	// Create interface to Tobii API
	tobii_error_t error = tobii_api_create(&api, nullptr, nullptr);
	assert(error == TOBII_ERROR_NO_ERROR);
	error = tobii_system_clock(api, &last_sync_time);
	assert(error == TOBII_ERROR_NO_ERROR);

	// Get list of devices.
	devices = list_devices(api, error);
	if (devices.size() == 0)
	{
		std::cerr << "No stream engine compatible device(s) found." << std::endl;
	}

	// Find device matching device_url
	if (std::find(devices.begin(), devices.end(), device_url) != devices.end())
	{
		selected_device = device_url;
	}
	else {
	}

	error = device_connect(api, selected_device, retries, interval, &device);
	assert(error == TOBII_ERROR_NO_ERROR);

	tobii_device_info_t device_info;
	error = tobii_get_device_info(device, &device_info);
	assert(error == TOBII_ERROR_NO_ERROR);

	/*
	tobii_supported_t supported;
	error = tobii_stream_supported(device, TOBII_STREAM_GAZE_POINT, &supported);
	assert(error == TOBII_ERROR_NO_ERROR);
	if (supported == TOBII_SUPPORTED)
		std::cout << "Device supports gaze point stream." << std::endl;
	else
		std::cout << "Device does not support gaze point stream." << std::endl;
	*/

	// create an outlet for gaze_point data
	lsl::stream_info info(name + "_gaze", "gaze", 2, lsl::IRREGULAR_RATE, lsl::cf_float32, std::string(device_info.serial_number) + "_gaze");
	info.desc().append_child_value("manufacturer", "Tobii");
	info.desc().append_child_value("model", device_info.model);
	info.desc().append_child_value("serial", device_info.serial_number);
	info.desc().append_child_value("generation", device_info.generation);
	lsl::xml_element chns = info.desc().append_child("channels");
	for each (std::string chan_name in std::vector<std::string>{ "x", "y" })
		chns.append_child("channel")
		.append_child_value("label", chan_name)
		.append_child_value("unit", "normalized")
		.append_child_value("type", "screen");
	lsl::stream_outlet gaze_outlet(info);

	/*
	error = tobii_stream_supported(device, TOBII_STREAM_WEARABLE_CONSUMER, &supported);
	assert(error == TOBII_ERROR_NO_ERROR);
	if (supported == TOBII_SUPPORTED)
		std::cout << "Device supports wearable stream." << std::endl;
	else
		std::cout << "Device does not support wearable stream." << std::endl;
	*/

	// Start subscribing to gaze and supply lambda callback function to handle the gaze point data
	error = tobii_gaze_point_subscribe(device,
		[](tobii_gaze_point_t const* gaze_point, void* user_data)
		{
			auto p_outlet = (lsl::stream_outlet*)user_data;  // Cast user_data back to outlet pointer
			double timestamp_s = (double)gaze_point->timestamp_us / 1000000;
			if (gaze_point->validity == TOBII_VALIDITY_VALID)
			{
				std::vector<float> sample = { gaze_point->position_xy[0], gaze_point->position_xy[1] };
				p_outlet->push_sample(sample, timestamp_s);
			}
			else
			{
				std::vector<float> sample = { NAN, NAN };
				p_outlet->push_sample(sample, timestamp_s);
			}
		}, &gaze_outlet);
	assert(error == TOBII_ERROR_NO_ERROR);

	// create an outlet for Tobii events
	lsl::stream_info event_info(name + "_events", "gaze_events", 1, lsl::IRREGULAR_RATE, lsl::cf_float32, std::string(device_info.serial_number) + "_events");
	info.desc().append_child_value("manufacturer", "Tobii");
	info.desc().append_child_value("model", device_info.model);
	info.desc().append_child_value("serial", device_info.serial_number);
	info.desc().append_child_value("generation", device_info.generation);
	lsl::stream_outlet event_outlet(event_info);
	error = tobii_user_presence_subscribe(device, 
		[](tobii_user_presence_status_t status, int64_t timestamp_us, void* user_data) {
			auto p_outlet = (lsl::stream_outlet*)user_data;  // Cast user_data back to outlet pointer
			double timestamp_s = (double)timestamp_us / 1000000;
			switch (status)
			{
			case TOBII_USER_PRESENCE_STATUS_UNKNOWN:
				p_outlet->push_sample("USER_PRESENCE_STATUS_UNKNOWN", timestamp_s);
				break;
			case TOBII_USER_PRESENCE_STATUS_AWAY:
				p_outlet->push_sample("USER_PRESENCE_STATUS_AWAY", timestamp_s);
				break;
			case TOBII_USER_PRESENCE_STATUS_PRESENT:
				p_outlet->push_sample("USER_PRESENCE_STATUS_PRESENT", timestamp_s);
				break;
			default:
				break;
			}
		}, &event_outlet);
	assert(error == TOBII_ERROR_NO_ERROR);
	error = tobii_notifications_subscribe(device,
		[](tobii_notification_t const* notification, void* user_data) {
			auto p_outlet = (lsl::stream_outlet*)user_data;  // Cast user_data back to outlet pointer
			std::string ev_str;
			switch (notification->type)
			{
			case TOBII_NOTIFICATION_TYPE_CALIBRATION_STATE_CHANGED:
				if (notification->value.state == TOBII_STATE_BOOL_TRUE)
					ev_str = "Calibration started";
				else
					ev_str = "Calibration stopped";
				break;
			case TOBII_NOTIFICATION_TYPE_FRAMERATE_CHANGED:
				ev_str = "FRAMERATE_CHANGED";
				break;
			case TOBII_NOTIFICATION_TYPE_DEVICE_PAUSED_STATE_CHANGED:
				if (notification->value.state == TOBII_STATE_BOOL_TRUE)
					ev_str = "Paused";
				else
					ev_str = "Unpaused";
				break;
			case TOBII_NOTIFICATION_TYPE_CALIBRATION_ENABLED_EYE_CHANGED:
				ev_str = "CALIBRATION_ENABLED_EYE_CHANGED";
				break;
			case TOBII_NOTIFICATION_TYPE_COMBINED_GAZE_EYE_SELECTION_CHANGED:
				ev_str = "COMBINED_GAZE_EYE_SELECTION_CHANGED";
				break;
			case TOBII_NOTIFICATION_TYPE_CALIBRATION_ID_CHANGED:
				ev_str = "Calibration ID changed to " + std::to_string(notification->value.uint_);
				break;
			default:
				break;
			}
			if (ev_str != "")
				p_outlet->push_sample(ev_str.c_str());
		}, &event_outlet);
	assert(error == TOBII_ERROR_NO_ERROR);

	while (!shutdown) {
		// Every ~30 seconds, update the timesync
		int64_t current_time;
		error = tobii_system_clock(api, &current_time);
		if ((current_time - last_sync_time) > 30000000)
		{
			tobii_update_timesync(device);
			last_sync_time = current_time;
		}

		auto error = tobii_wait_for_callbacks(1, &device);

		if (error == TOBII_ERROR_TIMED_OUT) continue; // If timed out, redo the wait for callbacks call
		else if (error != TOBII_ERROR_NO_ERROR)
		{
			std::cerr << "tobii_wait_for_callbacks failed: " << tobii_error_message(error) << "." << std::endl;
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
				std::cerr << "Connection was lost and reconnection failed." << std::endl;
				break;
			}
			continue;
		}
		else if (error != TOBII_ERROR_NO_ERROR)
		{
			std::cerr << "tobii_device_process_callbacks failed: " << tobii_error_message(error) << "." << std::endl;
			break;
		}

	}

	error = tobii_notifications_unsubscribe(device);
	assert(error == TOBII_ERROR_NO_ERROR);
	error = tobii_user_presence_unsubscribe(device);
	assert(error == TOBII_ERROR_NO_ERROR);
	error = tobii_gaze_point_unsubscribe(device);
	assert(error == TOBII_ERROR_NO_ERROR);
	error = tobii_device_destroy(device);
	assert(error == TOBII_ERROR_NO_ERROR);
	error = tobii_api_destroy(api);
	assert(error == TOBII_ERROR_NO_ERROR);
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
