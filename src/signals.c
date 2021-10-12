/*
Copyright (c) 2016-2020 Roger Light <roger@atchoo.org>

All rights reserved. This program and the accompanying materials
are made available under the terms of the Eclipse Public License 2.0
and Eclipse Distribution License v1.0 which accompany this distribution.
 
The Eclipse Public License is available at
   https://www.eclipse.org/legal/epl-2.0/
and the Eclipse Distribution License is available at
  http://www.eclipse.org/org/documents/edl-v10.php.
 
SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

Contributors:
   Roger Light - initial implementation and documentation.
   Dmitry Kaukov - windows named events implementation.
*/
#ifdef WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

#include "config.h"

#include <stdio.h>
#include <stdbool.h>
#include <signal.h>

#include "mosquitto_broker_internal.h"

extern int g_run;

bool flag_reload = false;
#ifdef WITH_PERSISTENCE
bool flag_db_backup = false;
#endif
bool flag_tree_print = false;

static void handle_signal(int signal)
{
	UNUSED(signal);

	if(signal == SIGINT || signal == SIGTERM){
		g_run = 0;
#ifdef SIGHUP
	}else if(signal == SIGHUP){
		flag_reload = true;
#endif
	}else if(signal == SIGUSR1){
#ifdef WITH_PERSISTENCE
		flag_db_backup = true;
#endif
	}else if(signal == SIGUSR2){
		flag_tree_print = true;
	}
}


void signal__setup(void)
{
	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);
#ifdef SIGHUP
	signal(SIGHUP, handle_signal);
#endif
#ifndef WIN32
	signal(SIGUSR1, handle_signal);
	signal(SIGUSR2, handle_signal);
	signal(SIGPIPE, SIG_IGN);
#endif
#ifdef WIN32
	CreateThread(NULL, 0, SigThreadProc, NULL, 0, NULL);
#endif
}

/*
 *
 * Signalling mosquitto process on Win32.
 *
 *  On Windows we we can use named events to pass signals to the mosquitto process.
 *  List of events :
 *
 *    mosqPID_shutdown
 *    mosqPID_reload
 *    mosqPID_backup
 *
 * (where PID is the PID of the mosquitto process).
 */
#ifdef WIN32
DWORD WINAPI SigThreadProc(void* data)
{
	TCHAR evt_name[MAX_PATH];
	static HANDLE evt[3];
	int pid = GetCurrentProcessId();

	UNUSED(data);

	sprintf_s(evt_name, MAX_PATH, "mosq%d_shutdown", pid);
	evt[0] = CreateEvent(NULL, TRUE, FALSE, evt_name);
	sprintf_s(evt_name, MAX_PATH, "mosq%d_reload", pid);
	evt[1] = CreateEvent(NULL, FALSE, FALSE, evt_name);
	sprintf_s(evt_name, MAX_PATH, "mosq%d_backup", pid);
	evt[2] = CreateEvent(NULL, FALSE, FALSE, evt_name);

	while (true) {
		int wr = WaitForMultipleObjects(sizeof(evt) / sizeof(HANDLE), evt, FALSE, INFINITE);
		switch (wr) {
			case WAIT_OBJECT_0 + 0:
				handle_sigint(SIGINT);
				break;
			case WAIT_OBJECT_0 + 1:
				flag_reload = true;
				continue;
			case WAIT_OBJECT_0 + 2:
				handle_sigusr1(0);
				continue;
				break;
		}
	}
	CloseHandle(evt[0]);
	CloseHandle(evt[1]);
	CloseHandle(evt[2]);
	return 0;
}
#endif
