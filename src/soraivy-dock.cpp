/*
Soraivy for OBS
Copyright (C) 2026 Soraivy, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "soraivy-dock.hpp"

#include "soraivy-dock.h"

extern "C" {
#include "soraivy-api.h"
#include "soraivy-json.h"
}

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#include <cstdio>
#include <cstring>
#include <thread>

#define SORAIVY_DOCK_ID "soraivyDock"
#define SORAIVY_DOCK_TITLE "Soraivy"
#define SORAIVY_DEFAULT_API_BASE "https://www.soraivy.com"

namespace {

bool json_escape(const char *src, char *dst, size_t size)
{
	size_t n = 0;

	if (!src || !dst || size == 0)
		return false;
	dst[0] = '\0';

	for (; *src != '\0'; src++) {
		unsigned char c = (unsigned char)*src;

		if (c == '"' || c == '\\') {
			if (n + 2 >= size)
				return false;
			dst[n++] = '\\';
			dst[n++] = (char)c;
		} else if (c < 0x20) {
			continue;
		} else {
			if (n + 1 >= size)
				return false;
			dst[n++] = (char)c;
		}
	}

	dst[n] = '\0';
	return true;
}

/* Same connect sequence as the service panel: creds fetch, then a
 * broadcast-create fallback when no id comes back. True with creds filled. */
bool fetch_creds(const std::string &api_base, const std::string &token, const std::string &title, bool whip,
		 struct soraivy_creds *creds)
{
	char body[SORAIVY_API_BODY_MAX];
	char req[2048];
	char title_esc[1024];
	long status = 0;

	if (soraivy_api_get(api_base.c_str(), token.c_str(), "/api/live/encoder-credentials", &status, body,
			    sizeof(body)) != 0) {
		return false;
	}
	if (status == 401 || status != 200 || !soraivy_parse_encoder_creds(body, creds))
		return false;

	if (creds->broadcast_id[0] != '\0')
		return true;

	if (!json_escape(title.c_str(), title_esc, sizeof(title_esc)))
		return false;
	snprintf(req, sizeof(req),
		 "{\"title\":\"%s\",\"videoQuality\":\"1080p\","
		 "\"videoFps\":%d,\"latencyMode\":\"low\"}",
		 title_esc, whip ? 60 : 30);
	if (soraivy_api_post(api_base.c_str(), token.c_str(), "/api/live/broadcasts", req, &status, body,
			     sizeof(body)) != 0 ||
	    (status != 200 && status != 201) || !soraivy_parse_broadcast_created(body, creds)) {
		return false;
	}
	return true;
}

} // namespace

SoraivyDock::SoraivyDock(QWidget *parent) : QWidget(parent)
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(8, 8, 8, 8);
	layout->setSpacing(6);

	apiBaseEdit = new QLineEdit(QString::fromStdString(SORAIVY_DEFAULT_API_BASE), this);
	tokenEdit = new QLineEdit(this);
	tokenEdit->setEchoMode(QLineEdit::Password);
	tokenEdit->setPlaceholderText("OBS Token (secret)");
	titleEdit = new QLineEdit("Live", this);
	modeBox = new QComboBox(this);
	modeBox->addItem("RTMPS — 30fps HLS", "rtmps");
	modeBox->addItem("WHIP — 60fps (OBS 31+)", "whip");

	layout->addWidget(new QLabel("API Base", this));
	layout->addWidget(apiBaseEdit);
	layout->addWidget(new QLabel("OBS Token", this));
	layout->addWidget(tokenEdit);
	layout->addWidget(new QLabel("Stream title (new session)", this));
	layout->addWidget(titleEdit);
	layout->addWidget(new QLabel("Mode", this));
	layout->addWidget(modeBox);

	QHBoxLayout *btnRow = new QHBoxLayout();
	connectBtn = new QPushButton("Connect", this);
	goLiveBtn = new QPushButton("Go Live", this);
	endBtn = new QPushButton("End", this);
	btnRow->addWidget(connectBtn);
	btnRow->addWidget(goLiveBtn);
	btnRow->addWidget(endBtn);
	layout->addLayout(btnRow);

	statusLabel = new QLabel("Not connected.", this);
	statusLabel->setWordWrap(true);
	layout->addWidget(statusLabel);

	serverLabel = new QLabel("Server: —", this);
	serverLabel->setWordWrap(true);
	serverLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	layout->addWidget(serverLabel);

	keyLabel = new QLabel("Key: —", this);
	keyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	layout->addWidget(keyLabel);

	QHBoxLayout *copyRow = new QHBoxLayout();
	QPushButton *copyServerBtn = new QPushButton("Copy server", this);
	QPushButton *copyKeyBtn = new QPushButton("Copy key", this);
	copyRow->addWidget(copyServerBtn);
	copyRow->addWidget(copyKeyBtn);
	layout->addLayout(copyRow);

	watchLabel = new QLabel(this);
	watchLabel->setOpenExternalLinks(true);
	watchLabel->setWordWrap(true);
	layout->addWidget(watchLabel);
	layout->addStretch(1);

	connect(connectBtn, &QPushButton::clicked, this, &SoraivyDock::onConnectClicked);
	connect(goLiveBtn, &QPushButton::clicked, this, &SoraivyDock::onGoLiveClicked);
	connect(endBtn, &QPushButton::clicked, this, &SoraivyDock::onEndClicked);
	connect(copyServerBtn, &QPushButton::clicked, this, &SoraivyDock::onCopyServer);
	connect(copyKeyBtn, &QPushButton::clicked, this, &SoraivyDock::onCopyKey);

	loadPersisted();
}

SoraivyDock::~SoraivyDock()
{
	gone->store(true);
}

void SoraivyDock::setBusy(bool busy)
{
	connectBtn->setEnabled(!busy);
	goLiveBtn->setEnabled(!busy);
	endBtn->setEnabled(!busy);
}

void SoraivyDock::loadPersisted()
{
	/* Qt-native persistence: obs_frontend_get_global_config is deprecated. */
	QSettings cfg("Soraivy", "obs-dock");
	apiBaseEdit->setText(cfg.value("ApiBase", SORAIVY_DEFAULT_API_BASE).toString());
	tokenEdit->setText(cfg.value("Token").toString());
	QString title = cfg.value("Title").toString();
	if (!title.isEmpty())
		titleEdit->setText(title);
	if (cfg.value("Mode").toString() == "whip")
		modeBox->setCurrentIndex(1);
	QString bid = cfg.value("BroadcastId").toString();
	if (!bid.isEmpty()) {
		broadcastId = bid.toStdString();
		statusLabel->setText("Restored previous session. Press Connect to refresh.");
	}
}

void SoraivyDock::savePersisted() const
{
	QSettings cfg("Soraivy", "obs-dock");
	cfg.setValue("ApiBase", apiBaseEdit->text().trimmed());
	cfg.setValue("Token", tokenEdit->text());
	cfg.setValue("Title", titleEdit->text());
	cfg.setValue("Mode", modeBox->currentData().toString());
	cfg.setValue("BroadcastId", QString::fromStdString(broadcastId));
}

void SoraivyDock::onConnectClicked()
{
	if (tokenEdit->text().trimmed().isEmpty()) {
		statusLabel->setText("Paste your OBS Token first (Soraivy Settings → Streaming).");
		return;
	}
	setBusy(true);
	statusLabel->setText("Connecting…");

	std::string apiBase = apiBaseEdit->text().trimmed().toStdString();
	std::string token = tokenEdit->text().toStdString();
	std::string title = titleEdit->text().toStdString();
	bool whip = modeBox->currentData().toString() == "whip";

	std::thread([this, gone = gone, apiBase, token, title, whip]() {
		struct soraivy_creds creds;
		soraivy_creds_init(&creds);
		if (!fetch_creds(apiBase, token, title, whip, &creds)) {
			if (!gone->load())
				QMetaObject::invokeMethod(this, "onActionError", Qt::QueuedConnection,
							  Q_ARG(QString, "Connect failed. Check the token."));
			return;
		}
		QString server = whip ? QString::fromUtf8(creds.whip_url) : QString::fromUtf8(creds.rtmps_url);
		QString key = whip ? QString() : QString::fromUtf8(creds.stream_key);
		QString watch = QString::fromUtf8(creds.watch_path);
		QString bid = QString::fromUtf8(creds.broadcast_id);
		if (!gone->load())
			QMetaObject::invokeMethod(this, "onConnectDone", Qt::QueuedConnection, Q_ARG(QString, server),
						  Q_ARG(QString, key), Q_ARG(QString, watch), Q_ARG(QString, bid));
	}).detach();
}

void SoraivyDock::onConnectDone(const QString &server, const QString &key, const QString &path, const QString &bid)
{
	lastServer = server.toStdString();
	lastKey = key.toStdString();
	broadcastId = bid.toStdString();
	watchPath = path.toStdString();

	serverLabel->setText("Server: " + server);
	keyLabel->setText(key.isEmpty() ? "Key: (WHIP needs no key)" : "Key: •••••••• (Copy key)");

	QString apiBase = apiBaseEdit->text().trimmed();
	QString where = QString::fromStdString(watchPath);
	watchLabel->setText(QString("<a href=\"%1%2\">Open watch page (%2)</a>").arg(apiBase, where));

	/* Push creds into the Soraivy streaming service so Start Streaming just works. */
	QString note;
	obs_service_t *svc = obs_frontend_get_streaming_service();
	if (svc && strcmp(obs_service_get_id(svc), "soraivy") == 0) {
		obs_data_t *settings = obs_service_get_settings(svc);
		obs_data_set_string(settings, "server", lastServer.c_str());
		obs_data_set_string(settings, "key", lastKey.c_str());
		obs_data_set_string(settings, "bearer_token", "");
		obs_service_update(svc, settings);
		obs_data_release(settings);
		obs_frontend_save_streaming_service();
		note = "Connected and applied to the Soraivy service.";
	} else {
		note = "Connected. Select Settings → Stream → Soraivy to auto-apply.";
	}
	statusLabel->setText(note);
	savePersisted();
	setBusy(false);
	blog(LOG_INFO, "[soraivy] dock connected. Watch: %s%s", apiBaseEdit->text().toUtf8().constData(),
	     watchPath.c_str());
}

void SoraivyDock::onGoLiveClicked()
{
	if (broadcastId.empty()) {
		statusLabel->setText("Press Connect first.");
		return;
	}
	setBusy(true);
	statusLabel->setText("Going live…");

	std::string apiBase = apiBaseEdit->text().trimmed().toStdString();
	std::string token = tokenEdit->text().toStdString();
	std::string bid = broadcastId;

	std::thread([this, gone = gone, apiBase, token, bid]() {
		char body[SORAIVY_API_BODY_MAX];
		char path[256];
		char statusText[64];
		long status = 0;
		snprintf(path, sizeof(path), "/api/live/broadcasts/%s/go-live", bid.c_str());
		if (soraivy_api_post(apiBase.c_str(), token.c_str(), path, "{}", &status, body, sizeof(body)) != 0) {
			if (!gone->load())
				QMetaObject::invokeMethod(this, "onActionError", Qt::QueuedConnection,
							  Q_ARG(QString, "Go-live failed."));
			return;
		}
		if (status == 200 && soraivy_json_get_string(body, "status", statusText, sizeof(statusText)) &&
		    strcmp(statusText, "live") == 0) {
			if (!gone->load())
				QMetaObject::invokeMethod(this, "onActionDone", Qt::QueuedConnection,
							  Q_ARG(QString, "LIVE — share your watch page."),
							  Q_ARG(bool, true));
			return;
		}
		char err[SORAIVY_ERROR_SIZE];
		QString msg = soraivy_parse_error(body, err, sizeof(err)) ? QString::fromUtf8(err) : "Go-live failed.";
		if (!gone->load())
			QMetaObject::invokeMethod(this, "onActionError", Qt::QueuedConnection, Q_ARG(QString, msg));
	}).detach();
}

void SoraivyDock::onEndClicked()
{
	if (broadcastId.empty()) {
		statusLabel->setText("Nothing to end.");
		return;
	}
	setBusy(true);
	statusLabel->setText("Ending…");

	std::string apiBase = apiBaseEdit->text().trimmed().toStdString();
	std::string token = tokenEdit->text().toStdString();
	std::string bid = broadcastId;

	std::thread([this, gone = gone, apiBase, token, bid]() {
		char body[SORAIVY_API_BODY_MAX];
		char path[256];
		long status = 0;
		snprintf(path, sizeof(path), "/api/live/broadcasts/%s/end", bid.c_str());
		if (soraivy_api_post(apiBase.c_str(), token.c_str(), path, "{}", &status, body, sizeof(body)) != 0 ||
		    status != 200) {
			if (!gone->load())
				QMetaObject::invokeMethod(this, "onActionError", Qt::QueuedConnection,
							  Q_ARG(QString, "End failed."));
			return;
		}
		if (!gone->load())
			QMetaObject::invokeMethod(this, "onActionDone", Qt::QueuedConnection,
						  Q_ARG(QString, "Stream ended."), Q_ARG(bool, false));
	}).detach();
}

void SoraivyDock::onActionDone(const QString &message, bool live)
{
	if (!live)
		broadcastId.clear();
	statusLabel->setText(QString("%1%2").arg(message, live ? "" : " Press Connect for the next one."));
	savePersisted();
	setBusy(false);
	blog(LOG_INFO, "[soraivy] dock: %s", message.toUtf8().constData());
}

void SoraivyDock::onActionError(const QString &message)
{
	statusLabel->setText(message);
	setBusy(false);
	blog(LOG_WARNING, "[soraivy] dock: %s", message.toUtf8().constData());
}

void SoraivyDock::onCopyServer()
{
	if (lastServer.empty()) {
		statusLabel->setText("Press Connect first.");
		return;
	}
	QApplication::clipboard()->setText(QString::fromStdString(lastServer));
	statusLabel->setText("Server copied.");
}

void SoraivyDock::onCopyKey()
{
	if (lastKey.empty()) {
		statusLabel->setText("Press Connect first.");
		return;
	}
	QApplication::clipboard()->setText(QString::fromStdString(lastKey));
	statusLabel->setText("Key copied.");
}

static SoraivyDock *soraivy_dock_widget = nullptr;

extern "C" void soraivy_dock_init(void)
{
	if (soraivy_dock_widget)
		return;
	soraivy_dock_widget = new SoraivyDock();
	if (!obs_frontend_add_dock_by_id(SORAIVY_DOCK_ID, SORAIVY_DOCK_TITLE, soraivy_dock_widget)) {
		blog(LOG_WARNING, "[soraivy] dock registration failed");
		delete soraivy_dock_widget;
		soraivy_dock_widget = nullptr;
		return;
	}
	blog(LOG_INFO, "[soraivy] dock added (View -> Docks -> Soraivy)");
}

extern "C" void soraivy_dock_free(void)
{
	if (!soraivy_dock_widget)
		return;
	/* The frontend owns the widget after a successful add; removal drops the
	 * dock and its id from the UI. Just drop our pointer. */
	obs_frontend_remove_dock(SORAIVY_DOCK_ID);
	soraivy_dock_widget = nullptr;
}
