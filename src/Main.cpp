#include <Windows.h>
#include <stdint.h>
#include <stdio.h>
#include <cstdio>
#include <vulkan/vulkan.h>

#include "include/Core.h"

#include "include/Application.h"

#undef APIENTRY
int __stdcall WINAPI
wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nShowCmd
)
{
	(void)hPrevInstance;
	(void)lpCmdLine;

	VulkanApp* application = new VulkanApp(VulkanApp::ERenderMethod::Graphics, hInstance, nShowCmd);
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
