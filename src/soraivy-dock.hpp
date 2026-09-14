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

#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

/* Persistent OBS dock (View -> Docks -> Soraivy): token + mode + title,
 * Connect / Go Live / End, server/key display with copy, watch link.
 * Network runs on detached worker threads; every obs_* call happens on the
 * UI thread via queued slots. Secrets are never logged. */
class SoraivyDock : public QWidget {
	Q_OBJECT

public:
	explicit SoraivyDock(QWidget *parent = nullptr);
	~SoraivyDock() override;

private slots:
	void onConnectClicked();
	void onGoLiveClicked();
	void onEndClicked();
	void onCopyServer();
	void onCopyKey();
	void onConnectDone(const QString &server, const QString &key, const QString &path, const QString &bid);
	void onActionDone(const QString &message, bool live);
	void onActionError(const QString &message);

private:
	void setBusy(bool busy);
	void loadPersisted();
	void savePersisted() const;

	QLineEdit *apiBaseEdit = nullptr;
	QLineEdit *tokenEdit = nullptr;
	QLineEdit *titleEdit = nullptr;
	QComboBox *modeBox = nullptr;
	QPushButton *connectBtn = nullptr;
	QPushButton *goLiveBtn = nullptr;
	QPushButton *endBtn = nullptr;
	QLabel *statusLabel = nullptr;
	QLabel *serverLabel = nullptr;
	QLabel *keyLabel = nullptr;
	QLabel *watchLabel = nullptr;

	std::string broadcastId;
	std::string watchPath = "/go-live";
	std::string lastServer;
	std::string lastKey;

	/* Set in the destructor; workers hold a copy and skip UI callbacks once set. */
	std::shared_ptr<std::atomic<bool>> gone = std::make_shared<std::atomic<bool>>(false);
};
