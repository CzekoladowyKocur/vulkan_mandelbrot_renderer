#include <Windows.h>
#include <stdint.h>
#include <stdio.h>
#include <cstdio>
#include <vulkan/vulkan.h>

#include "include/Core.h"

#include "include/Application.h"

#undef APIENTRY
INT WINAPI wWinMain(
	HINSTANCE hInstance,
	HINSTANCE hPreviousInstance,
	PWSTR pCmdLine,
	INT cmdShow)
{
	VulkanApp* application = new VulkanApp(VulkanApp::ERenderMethod::Graphics, hInstance, cmdShow);
	if (application->Initialize())
	{
		if (application->Run())
		{

		}
		else
		{
			printf("Failed to run application properly\n");
			delete application;
			return EXIT_FAILURE;
		}

		if (application->Shutdown())
		{
			/* Everything went well up to this point.. weird.. */
		}
		else
		{
			printf("Failed to shutdown application properly\n");
			delete application;
			return EXIT_FAILURE;
		}
	}
	else
	{
		printf("Failed to initialize application\n");
		delete application;
		return EXIT_FAILURE;
	} /* Application Initialize */

	printf("Shutting down. . .\n");
	delete application;
	return EXIT_SUCCESS;
}
