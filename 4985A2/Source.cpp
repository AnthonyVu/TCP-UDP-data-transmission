// Program Source.cpp
/*------------------------------------------------------------------------------------------------------------------
--
-- PROGRAM: 4985A2
--
-- DATE: February 12, 2018
--
-- DESIGNER: Anthony Vu
--
-- PROGRAMMER: Anthony Vu
--
-- NOTES: A program that allows a user to transfer a file using TCP or UDP to another user. The user may choose the packet
-- size and number of packets to send. If the remaining characters in the file is less than the packet size, it will still
-- be sent through. The user must specify the mode (Client or Server). Only the Client may send a file; the server will receive
-- the data and save it to a file, As well, the number of packets received and packet sizes will be displayed on the GUI.
-- Connecting, Accepting, and Reading is done asynchronously.
----------------------------------------------------------------------------------------------------------------------*/
#pragma comment(lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma warning(disable:4996)

#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include<Mswsock.h>
#include <stdio.h>
#include <conio.h>
#include <atlstr.h>
#include <time.h>

#include "dialog.h"
#include "menu.h"

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define STRICT
#define DATA_BUFSIZE 6000000
#define PACKET_BUFSIZE 100000
#define DEF_BUFSIZE 100
#define WM_SOCKET (WM_USER + 1)

typedef struct _SOCKET_INFORMATION {
	BOOL RecvPosted;
	CHAR Buffer[DATA_BUFSIZE];
	WSABUF DataBuf;
	SOCKET Socket;
	DWORD BytesSEND;
	DWORD BytesRECV;
	_SOCKET_INFORMATION *Next;
} SOCKET_INFORMATION, *LPSOCKET_INFORMATION;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
VOID CALLBACK FileIOCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransferred, LPOVERLAPPED lpOverlapped);
BOOL CALLBACK ToolDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK ClientProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK ServerProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LPSOCKET_INFORMATION GetSocketInformation(SOCKET s);
void CreateSocketInformation(SOCKET s);
void FreeSocketInformation(SOCKET s);
char* lowercase(char[]);
void setDefaultSettings();
long delay(SYSTEMTIME t1, SYSTEMTIME t2);

LPSOCKET_INFORMATION SocketInfoList;
HWND hwnd, dialog;
boolean settings;
OPENFILENAME ofn;
char inputFileBuffer[DATA_BUFSIZE] = { 0 };
OVERLAPPED ol;
DWORD g_BytesTransferred;

char filePathBuffer[DEF_BUFSIZE];
char mode[DEF_BUFSIZE];
char protocol[DEF_BUFSIZE];
char portNo[DEF_BUFSIZE];
char packetSize[DEF_BUFSIZE];
char maxPackets[DEF_BUFSIZE];
char ip[DEF_BUFSIZE];
int portConversion;
int numPackets;
int packetSizeConversion;
CString packetSizeReceived;
CString numPacketsReceived;

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: WinMain
--
-- DATE: February 12, 2018
--
-- DESIGNER: Anthony Vu
--
-- PROGRAMMER: Anthony Vu
--
-- INTERFACE: int WINAPI WinMain(HINSANCE hInst, HINSTANCE, hprevInstance, LPSTR lspszCmdParam, int cmdShow)
--
-- RETURNS: int
--
-- NOTES:
-- This function creates the window and sets up variables for file opening.
----------------------------------------------------------------------------------------------------------------------*/
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hprevInstance,
	LPSTR lspszCmdParam, int nCmdShow)
{
	char Name[] = "Assignment 2";
	MSG Msg;
	WNDCLASSEX Wcl;
	HINSTANCE hInstance;
	hInstance = hInst;
	Wcl.cbSize = sizeof(WNDCLASSEX);
	Wcl.style = CS_HREDRAW | CS_VREDRAW;
	Wcl.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	Wcl.hIconSm = NULL;
	Wcl.hCursor = LoadCursor(NULL, IDC_ARROW);

	Wcl.lpfnWndProc = WndProc;
	Wcl.hInstance = hInst;
	Wcl.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	Wcl.lpszClassName = Name;

	Wcl.lpszMenuName = "MYMENU";
	Wcl.cbClsExtra = 0;
	Wcl.cbWndExtra = 0;
	numPackets = 0;
	if (!RegisterClassEx(&Wcl))
		return 0;

	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFile = filePathBuffer;
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(filePathBuffer);
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
	ol = { 0 };

	hwnd = CreateWindow(Name, Name, WS_OVERLAPPEDWINDOW, 10, 10,
		600, 400, NULL, NULL, hInst, NULL);
	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);
	while (GetMessage(&Msg, NULL, 0, 0))
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}
	return Msg.wParam;
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: WndProc
--
-- DATE: February 12, 2018
--
-- DESIGNER: Anthony Vu
--
-- PROGRAMMER: Anthony Vu
--
-- INTERFACE: LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
--
-- RETURNS: LRESULT
--
-- NOTES:
-- Processes messages sent to a window, including socket events.
----------------------------------------------------------------------------------------------------------------------*/
LRESULT CALLBACK WndProc(HWND hwnd, UINT Message,
	WPARAM wParam, LPARAM lParam)
{
	WSADATA wsd;
	SOCKET ClientSocket, ServerSocket;
	SOCKADDR_IN server, client;
	SOCKET Connect;
	SOCKET Listen = NULL;
	HANDLE fileHandle;

	LPSOCKET_INFORMATION SocketInfo;
	DWORD RecvBytes, SendBytes;
	DWORD Flags;

	packetSizeConversion = atoi(packetSize);
	int bytesSent = 0;
	char packet[PACKET_BUFSIZE];
	switch (Message)
	{
	case WM_SOCKET:
		//If in client mode, packetize file and send to server
		if (strcmp(mode, "client") == 0) {
			ClientProc(hwnd, Message, wParam, lParam);
			WSAAsyncSelect(Listen, hwnd, WM_SOCKET, FD_WRITE | FD_CLOSE);
		}
		else {
			ServerProc(hwnd, Message, wParam, lParam);
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_SETTINGS:
			dialog = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG1),
				hwnd, ToolDlgProc);
			if (dialog != NULL)
			{
				ShowWindow(dialog, SW_SHOW);
				settings = true;
			}
			else
			{
				MessageBox(hwnd, "CreateDialog returned NULL", "Warning!",
					MB_OK | MB_ICONINFORMATION);
			}
			break;
		case ID_FILE:
			if (GetOpenFileName(&ofn) == TRUE)
			{
				fileHandle = CreateFile(ofn.lpstrFile, GENERIC_READ, 0, (LPSECURITY_ATTRIBUTES)NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, (HANDLE)NULL);

				if (fileHandle == INVALID_HANDLE_VALUE) {
					MessageBox(hwnd, "Error opening file", "", NULL);
				}

				if (!ReadFileEx(fileHandle, inputFileBuffer, sizeof(inputFileBuffer), &ol, FileIOCompletionRoutine))
				{
					MessageBox(hwnd, inputFileBuffer, "", NULL);
				}
				CloseHandle(fileHandle);
				numPackets = 0;
				InvalidateRect(hwnd, NULL, true);
			}
			break;
		case ID_START:
			setDefaultSettings();
			if (strcmp(lowercase(mode), "client") == 0 && strcmp(lowercase(protocol), "tcp") == 0) {
				WSAStartup(MAKEWORD(2, 2), &wsd);

				ClientSocket = socket(AF_INET, SOCK_STREAM, 0);
				WSAAsyncSelect(ClientSocket, hwnd, WM_SOCKET, FD_CONNECT | FD_CLOSE);
				if (ClientSocket == INVALID_SOCKET) {
					printf("socket() failed with error %d\n", WSAGetLastError());
				}

				client.sin_family = AF_INET;
				client.sin_addr.s_addr = inet_addr(ip);
				portConversion = atoi(portNo);
				client.sin_port = htons(portConversion);

				WSAAsyncSelect(Listen, hwnd, WM_SOCKET, FD_CONNECT | FD_CLOSE);
				Listen = connect(ClientSocket, (struct sockaddr far*)&client, sizeof(client));
				if (Listen == SOCKET_ERROR) {
					if ((Listen = WSAGetLastError()) !=
						WSAEWOULDBLOCK)
					{
						printf("connect() failed with error %d\n", WSAGetLastError());
						return(0);
					}
				}

			}
			else if (strcmp(lowercase(mode), "server") == 0 && strcmp(lowercase(protocol), "tcp") == 0) {
				WSAStartup(MAKEWORD(2, 2), &wsd);

				Listen = socket(PF_INET, SOCK_STREAM, 0);
				WSAAsyncSelect(Listen, hwnd, WM_SOCKET, FD_ACCEPT | FD_CLOSE);

				server.sin_family = AF_INET;
				server.sin_addr.s_addr = htonl(INADDR_ANY);
				portConversion = atoi(portNo);
				server.sin_port = htons(portConversion);

				if (bind(Listen, (PSOCKADDR)&server, sizeof(server)) == SOCKET_ERROR)
				{
					printf("bind() failed with error %d\n", WSAGetLastError());
				}
				// Set up window message notification on
				if (listen(Listen, 5))
				{
					printf("listen() failed with error %d\n", WSAGetLastError());
				}
				// the new socket using the WM_SOCKET define above
				//WSAAsyncSelect(ServerSocket, hwnd, WM_SOCKET, FD_ACCEPT | FD_READ | FD_CLOSE);
				listen(Listen, 5);
			}
			else if (strcmp(lowercase(mode), "client") == 0 && strcmp(lowercase(protocol), "udp") == 0) {
				WSAStartup(MAKEWORD(2, 2), &wsd);

				if ((ClientSocket = socket(AF_INET, SOCK_DGRAM, 0)) == SOCKET_ERROR) {
					printf("bind() failed with error %d\n", WSAGetLastError());
				}
				if (ClientSocket == INVALID_SOCKET) {
					printf("bind() failed with error %d\n", WSAGetLastError());
				}
				memset((char *)&client, 0, sizeof(client));
				client.sin_family = AF_INET;
				client.sin_addr.s_addr = htonl(INADDR_ANY);
				client.sin_port = htons(0);

				if ((bind(ClientSocket, (PSOCKADDR)&client, sizeof(client))) == SOCKET_ERROR) {
					printf("bind() failed with error %d\n", WSAGetLastError());
				}

				WSAAsyncSelect(ClientSocket, hwnd, WM_SOCKET, FD_WRITE | FD_CLOSE);
				ClientProc(hwnd, Message, wParam, lParam);
			}
			else if (strcmp(lowercase(mode), "server") == 0 && strcmp(lowercase(protocol), "udp") == 0) {
				WSAStartup(MAKEWORD(2, 2), &wsd);

				if ((Listen = socket(PF_INET, SOCK_DGRAM, 0)) == SOCKET_ERROR) {
					printf("socket() failed with error %d\n", WSAGetLastError());
				}
				WSAAsyncSelect(Listen, hwnd, WM_SOCKET, FD_READ | FD_CLOSE);

				server.sin_family = AF_INET;
				server.sin_addr.s_addr = htonl(INADDR_ANY);
				portConversion = atoi(portNo);
				server.sin_port = htons(portConversion);

				if ((bind(Listen, (PSOCKADDR)&server, sizeof(server))) == SOCKET_ERROR)
				{
					printf("bind() failed with error %d\n", WSAGetLastError());
				}
				CreateSocketInformation(Listen);
			}
			break;
		}
		break;
	case WM_PAINT:
		if (mode != NULL && strcmp(mode, "server") == 0) {
			PAINTSTRUCT ps;
			HDC hdc;
			hdc = BeginPaint(hwnd, &ps);
			TextOut(hdc, 0, 0, "Received packet of size : " + packetSizeReceived, strlen(packetSizeReceived) + strlen("Received packet of size : "));
			TextOut(hdc, 0, 20, "Number of Packets: " + numPacketsReceived, strlen(numPacketsReceived) + strlen("Number of Packets: "));
			EndPaint(hwnd, &ps);
		}
		break;
	case WM_DESTROY:
		closesocket((SOCKET)wParam);
		PostQuitMessage(0);
		break;
	default:
		closesocket((SOCKET)wParam);
		return DefWindowProc(hwnd, Message, wParam, lParam);
	}
	return 0;
}


/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: ClientProc
--
-- DATE: February 12, 2018
--
-- DESIGNER: Anthony Vu
--
-- PROGRAMMER: Anthony Vu
--
-- INTERFACE: BOOL CALLBACK ClientProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
--
-- RETURNS: BOOL
--
-- NOTES:
-- Processes messages sent to a socket on the client side
----------------------------------------------------------------------------------------------------------------------*/
BOOL CALLBACK ClientProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	LPSOCKET_INFORMATION	SocketInfo;
	DWORD					SendBytes;
	SOCKADDR_IN				server;
	int						packetSizeConversion = atoi(packetSize);
	int						bytesSent = 0;
	char					packet[PACKET_BUFSIZE];
	SYSTEMTIME				stStartTime, stEndTime;

	if (WSAGETSELECTERROR(lParam)) {
		printf("Socket failed with error %d", WSAGetLastError());
	}
	else {
		switch (WSAGETSELECTEVENT(lParam)) {
		case FD_CONNECT:
			CreateSocketInformation(wParam);
			WSAAsyncSelect(wParam, hwnd, WM_SOCKET, FD_WRITE | FD_CLOSE);
			break;
		case FD_WRITE:
			SocketInfo = GetSocketInformation(wParam);
			GetSystemTime(&stStartTime);
			if (strcmp(protocol, "tcp") == 0) {
				for (int i = 0; i < atoi(maxPackets); i++) {
					if (bytesSent < strlen(inputFileBuffer)) {
						//Sleep(50);
						memcpy(&packet, inputFileBuffer + bytesSent, packetSizeConversion);
						bytesSent += send(SocketInfo->Socket, packet, packetSizeConversion, 0);
						memset(packet, 0, sizeof(packet));
					}
				}
			}
			else {
				int packetSizeConversion = atoi(packetSize);
				memset((char *)&server, 0, sizeof(server));
				server.sin_family = AF_INET;
				server.sin_addr.s_addr = inet_addr(ip);
				portConversion = atoi(portNo);
				server.sin_port = htons(portConversion);
				int server_len = PACKET_BUFSIZE;
				for (int i = 0; i < atoi(maxPackets); i++) {
					if (bytesSent < strlen(inputFileBuffer)) {
						//Sleep(50);
						memcpy(&packet, inputFileBuffer + bytesSent, packetSizeConversion * sizeof(char));
						if ((sendto(wParam, packet, packetSizeConversion, 0, (struct sockaddr *)&server, server_len)) == -1) {
							printf("sendto() failed with error code : %d", WSAGetLastError());
						}
						bytesSent += packetSizeConversion;
						memset(packet, 0, sizeof(packet));
					}
				}
			}
			GetSystemTime(&stEndTime);
			printf("Round-trip delay = %ld ms.\n", delay(stStartTime, stEndTime));
			break;

		case FD_CLOSE:
			printf("Closing socket %d", wParam);
			FreeSocketInformation(wParam);
			break;
		}
		return FALSE;
	}

	return FALSE;
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: ServerProc
--
-- DATE: February 12, 2018
--
-- DESIGNER: Anthony Vu
--
-- PROGRAMMER: Anthony Vu
--
-- INTERFACE: BOOL CALLBACK ServerProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
--
-- RETURNS: BOOL
--
-- NOTES:
-- Processes messages sent to a socket on the server side
----------------------------------------------------------------------------------------------------------------------*/
BOOL CALLBACK ServerProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	SOCKET					Accept;
	LPSOCKET_INFORMATION	SocketInfo;
	DWORD					RecvBytes, SendBytes;
	DWORD					Flags;
	struct					sockaddr_in client;
	int						client_len;

	memset((char *)&client, 0, sizeof(struct sockaddr_in));

	if (WSAGETSELECTERROR(lParam)) {
		printf("Socket failed with error %d", WSAGetLastError());
	}
	else {
		switch (WSAGETSELECTEVENT(lParam)) {
		case FD_ACCEPT:
			client_len = sizeof(client);
			if ((Accept = accept(wParam, (struct sockaddr *)&client, &client_len)) == INVALID_SOCKET) {
				printf("accept() failed with error %d", WSAGetLastError());
				break;
			}
			CreateSocketInformation(Accept);
			WSAAsyncSelect(Accept, hWnd, WM_SOCKET, FD_READ | FD_CLOSE);
			break;
		case FD_READ:
			SocketInfo = GetSocketInformation(wParam);

			// Read data only if the receive buffer is empty.

			if (SocketInfo->BytesRECV != 0) {
				SocketInfo->RecvPosted = TRUE;
				return 0;
			}
			else {
				SocketInfo->DataBuf.buf = SocketInfo->Buffer;
				SocketInfo->DataBuf.len = packetSizeConversion;
				Flags = 0;
				//Sleep(10);
				// if protocol is udp
				if (strcmp(protocol, "udp") == 0) {
					if ((WSARecvFrom(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &RecvBytes, &Flags, NULL, NULL, NULL, NULL)) == SOCKET_ERROR) {
						if (WSAGetLastError() != WSAEWOULDBLOCK) {
							printf("WSARecv() failed with error %d", WSAGetLastError());
							FreeSocketInformation(wParam);
							return 0;
						}
					}
					else { // No error so update the byte count
						SocketInfo->BytesRECV = RecvBytes;
						FILE * infile;
						fopen_s(&infile, "udp.txt", "a");
						int n = fwrite(SocketInfo->DataBuf.buf, 1, strlen(SocketInfo->DataBuf.buf), infile);
						fclose(infile);
						packetSizeReceived.Format("%d", strlen(SocketInfo->DataBuf.buf));
						numPackets += 1;
						numPacketsReceived.Format("%d", numPackets);
						InvalidateRect(hwnd, NULL, true);
						WSAAsyncSelect(SocketInfo->Socket, hWnd, WM_SOCKET, FD_READ | FD_CLOSE);
					}
				}
				else { // if protocol is TCP
					if ((WSARecv(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &RecvBytes,
						&Flags, NULL, NULL)) == SOCKET_ERROR) {
						if (WSAGetLastError() != WSAEWOULDBLOCK) {
							printf("WSARecv() failed with error %d", WSAGetLastError());
							FreeSocketInformation(wParam);
							return 0;
						}
					}
					else { // No error so update the byte count
						SocketInfo->BytesRECV = RecvBytes;
						FILE * infile;
						fopen_s(&infile, "tcp.txt", "a");
						int n = fwrite(SocketInfo->DataBuf.buf, 1, strlen(SocketInfo->DataBuf.buf), infile);
						fclose(infile);
						packetSizeReceived.Format("%d", strlen(SocketInfo->DataBuf.buf));
						numPackets += 1;
						numPacketsReceived.Format("%d", numPackets);
						InvalidateRect(hwnd, NULL, true);
					}
				}
			}
			// Clear Buffer
			ZeroMemory(&(SocketInfo->Buffer), sizeof(SocketInfo));
			SocketInfo->DataBuf.buf = 0;
			SocketInfo->DataBuf.len = 0;
			SocketInfo->BytesRECV = 0;
			break;

		case FD_CLOSE:
			printf("Closing socket %d", wParam);
			break;
		}
		return FALSE;
	}

	return FALSE;
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: lowerCase
--
-- DATE: February 12, 2018
--
-- DESIGNER: Anthony Vu
--
-- PROGRAMMER: Anthony Vu
--
-- INTERFACE: char* lowercase(char string[])
--
-- RETURNS: char*
--
-- NOTES:
-- converts a string to all lowercase
----------------------------------------------------------------------------------------------------------------------*/
char* lowercase(char string[]) {
	for (int i = 0; i < 100; i++) {
		string[i] = tolower(string[i]);
	}
	return string;
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: setDefaultSettings
--
-- DATE: February 12, 2018
--
-- DESIGNER: Anthony Vu
--
-- PROGRAMMER: Anthony Vu
--
-- INTERFACE: void setDefaultSettings()
--
-- RETURNS: void
--
-- NOTES:
-- Sets default settings for program if the user decides not to select any.
----------------------------------------------------------------------------------------------------------------------*/
void setDefaultSettings() {
	if (strlen(mode) == 0) {
		strcpy_s(mode, "client");
	}
	if (strlen(protocol) == 0) {
		strcpy_s(protocol, "tcp");
	}
	if (strlen(portNo) == 0) {
		strcpy_s(portNo, "7000");
	}
	if (strlen(packetSize) == 0) {
		strcpy_s(packetSize, "60000");
	}
	if (strlen(maxPackets) == 0) {
		strcpy_s(maxPackets, "100");
	}
	if (strlen(ip) == 0) {
		strcpy_s(ip, "192.168.0.10");
	}
}
/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: FileIOCompletionRoutine
--
-- DATE: February 12, 2018
--
-- DESIGNER: Anthony Vu
--
-- PROGRAMMER: Anthony Vu
--
-- INTERFACE: VOID CALLBACK FileIOCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransferred, LPOVERLAPPED lpOverlapped)
--
-- RETURNS: void
--
-- NOTES:
-- Required for ReadFileEx function.
----------------------------------------------------------------------------------------------------------------------*/
VOID CALLBACK FileIOCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransferred, LPOVERLAPPED lpOverlapped)
{

}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: GetSocketInformation
--
-- DATE: February 12, 2018
--
-- DESIGNER: Unknown
--
-- PROGRAMMER: Unknown
--
-- INTERFACE: LPSOCKET_INFORMATION GetSocketInformation(SOCKET s)
--
-- RETURNS: void
--
-- NOTES:
-- Retrieves socket information struct from socket
----------------------------------------------------------------------------------------------------------------------*/
LPSOCKET_INFORMATION GetSocketInformation(SOCKET s)
{
	SOCKET_INFORMATION *SI = SocketInfoList;

	while (SI)
	{
		if (SI->Socket == s)
			return SI;

		SI = SI->Next;
	}

	return NULL;
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: CreateSocketInformation
--
-- DATE: February 12, 2018
--
-- DESIGNER: Unknown
--
-- PROGRAMMER: Unknown
--
-- INTERFACE: void CreateSocketInformation(SOCKET s)
--
-- RETURNS: void
--
-- NOTES:
-- Creates Socket info struct for socket
----------------------------------------------------------------------------------------------------------------------*/
void CreateSocketInformation(SOCKET s)
{
	LPSOCKET_INFORMATION SI;

	if ((SI = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		printf("GlobalAlloc() failed with error %d\n", GetLastError());
		return;
	}

	// Prepare SocketInfo structure for use.

	SI->Socket = s;
	SI->RecvPosted = FALSE;
	SI->BytesSEND = 0;
	SI->BytesRECV = 0;

	SI->Next = SocketInfoList;

	SocketInfoList = SI;
}
/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: FreeSocketInformation
--
-- DATE: February 12, 2018
--
-- DESIGNER: Unknown
--
-- PROGRAMMER: Unknown
--
-- INTERFACE: void FreeSocketInformation(SOCKET s)
--
-- RETURNS: void
--
-- NOTES:
-- Free Socket Information struct of socket
----------------------------------------------------------------------------------------------------------------------*/
void FreeSocketInformation(SOCKET s)
{
	SOCKET_INFORMATION *SI = SocketInfoList;
	SOCKET_INFORMATION *PrevSI = NULL;

	while (SI)
	{
		if (SI->Socket == s)
		{
			if (PrevSI)
				PrevSI->Next = SI->Next;
			else
				SocketInfoList = SI->Next;

			closesocket(SI->Socket);
			GlobalFree(SI);
			return;
		}

		PrevSI = SI;
		SI = SI->Next;
	}
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: ToolDlgProc
--
-- DATE: February 12, 2018
--
-- DESIGNER: Anthony Vu
--
-- PROGRAMMER: Anthony Vu
--
-- INTERFACE: BOOL CALLBACK ToolDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
--
-- RETURNS: BOOL
--
-- NOTES:
-- Processes messages sent to the dialog box in settings
----------------------------------------------------------------------------------------------------------------------*/
BOOL CALLBACK ToolDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	if (settings) {
		switch (Message)
		{
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
			case IDOK:
				GetDlgItemText(hwnd, IDC_EDIT1, mode, 100);
				GetDlgItemText(hwnd, IDC_EDIT2, protocol, 100);
				GetDlgItemText(hwnd, IDC_EDIT3, portNo, 100);
				GetDlgItemText(hwnd, IDC_EDIT4, packetSize, 100);
				GetDlgItemText(hwnd, IDC_EDIT5, maxPackets, 100);
				GetDlgItemText(hwnd, IDC_EDIT6, ip, 100);
			case IDCANCEL:
				settings = false;
				ShowWindow(dialog, SW_HIDE);
				return FALSE;
			}
			break;
		default:
			return DefWindowProc(hwnd, Message, wParam, lParam);
		}
		return TRUE;
	}
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: ToolDlgProc
--
-- DATE: January 6, 2008
--
-- DESIGNER: Aman Abdulla
--
-- PROGRAMMER: Aman Abdulla
--
-- INTERFACE: long delay(SYSTEMTIME t1, SYSTEMTIME t2)
--
-- RETURNS: long
--
-- NOTES:
-- Compute the delay between tl and t2 in milliseconds
----------------------------------------------------------------------------------------------------------------------*/
long delay(SYSTEMTIME t1, SYSTEMTIME t2)
{
	long d;

	d = (t2.wSecond - t1.wSecond) * 1000;
	d += (t2.wMilliseconds - t1.wMilliseconds);
	return(d);
}