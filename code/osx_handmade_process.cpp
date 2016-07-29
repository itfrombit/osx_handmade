///////////////////////////////////////////////////////////////////////
// osx_handmade_process.cpp
//
// Jeff Buck
// Copyright 2014-2016. All Rights Reserved.
//


///////////////////////////////////////////////////////////////////////
// External Process Control

#define STRINGIFY(S) #S

#if HANDMADE_INTERNAL
DEBUG_PLATFORM_EXECUTE_SYSTEM_COMMAND(DEBUGExecuteSystemCommand)
{
    debug_executing_process Result = {};

	char* Args[] = {"/bin/sh",
					STRINGIFY(DYNAMIC_COMPILE_COMMAND),
	                0};

	char* WorkingDirectory = STRINGIFY(DYNAMIC_COMPILE_PATH);

	int PID = fork();

	switch(PID)
	{
		case -1:
			printf("Error forking process: %d\n", PID);
			break;

		case 0:
		{
			// child
			chdir(WorkingDirectory);
			int ExecCode = execvp(Args[0], Args);
			if (ExecCode == -1)
			{
				printf("Error in execve: %d\n", errno);
			}
			break;
		}

		default:
			// parent
			printf("Launched child process %d\n", PID);
			break;
	}


	Result.OSHandle = PID;

    return(Result);
}


DEBUG_PLATFORM_GET_PROCESS_STATE(DEBUGGetProcessState)
{
	debug_process_state Result = {};

	int PID = (int)Process.OSHandle;
	int ExitCode = 0;

	if (PID > 0)
	{
		Result.StartedSuccessfully = true;
	}

	if (waitpid(PID, &ExitCode, WNOHANG) == PID)
	{
		Result.ReturnCode = WEXITSTATUS(ExitCode);
		printf("Child process %d exited with code %d...\n", PID, ExitCode);
	}
	else
	{
		Result.IsRunning = true;
	}

    return(Result);
}
#endif


