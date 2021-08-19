#pragma once
#include <Windows.h>
#include <assert.h>
#include "MainWindow.h"
#include "ImageResources.h"
#include "FontResources.h"
#include "../../installer/installer_base.h"
#include "../../installer/settings.h"

class Application
{
public:
	Application(HINSTANCE hInstance, int nCmdShow, bool isAutoUpdateMode);
	virtual ~Application();

	bool init(int windowCenterX, int windowCenterY);
	int exec();

	HINSTANCE getInstance() { return hInstance_;  }
	int getCmdShow() { return nCmdShow_;  }

	ImageResources *getImageResources() { assert(imageResources_ != NULL); return imageResources_; }
	FontResources *getFontResources() { assert(fontResources_ != NULL); return fontResources_; }

    InstallerBase *getInstaller() { return installer_.get();  }
	Settings &getSettings() { return settings_; }

private:
	ULONG_PTR gdiplusToken_;
	HINSTANCE hInstance_;
	int nCmdShow_;
	bool isAutoUpdateMode_;

	MainWindow *mainwWindow_;
	ImageResources *imageResources_;
	FontResources *fontResources_;
	std::unique_ptr<InstallerBase> installer_;
	Settings settings_;
    bool isLegacyOS_;

	void installerCallback(unsigned int progress, INSTALLER_CURRENT_STATE state);
};

extern Application *g_application;