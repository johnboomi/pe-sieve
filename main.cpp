// Scans for modified modules within the process of a given PID
// author: hasherezade (hasherezade@gmail.com)

#include <Windows.h>
#include <Psapi.h>
#include <sstream>
#include <fstream>

#include "hook_scanner.h"
#include "hollowing_scanner.h"
#include "process_privilege.h"

#include "util.h"

#include "peconv.h"
#include "pe_sieve.h"

#define PARAM_PID "/pid"
#define PARAM_FILTER "/filter"
#define PARAM_IMP_REC "/imp"
#define PARAM_NO_DUMP  "/nodump"
#define PARAM_HELP "/help"
#define PARAM_HELP2  "/?"
#define PARAM_VERSION  "/version"
#define PARAM_QUIET "/quiet"

void print_in_color(int color, std::string text)
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	FlushConsoleInputBuffer(hConsole);
	SetConsoleTextAttribute(hConsole, color); // back to default color
	std::cout << text;
	FlushConsoleInputBuffer(hConsole);
	SetConsoleTextAttribute(hConsole, 7); // back to default color
}

void print_help()
{
	const int hdr_color = 14;
	const int param_color = 15;
	print_in_color(hdr_color, "Required: \n");
	print_in_color(param_color, PARAM_PID);
	std::cout << " <target_pid>\n\t: Set the PID of the target process.\n";

	print_in_color(hdr_color, "\nOptional: \n");
	print_in_color(param_color, PARAM_IMP_REC);
	std::cout << "\t: Enable recovering imports. ";
	std::cout << "(Warning: it may slow down the scan)\n";
#ifdef _WIN64
	print_in_color(param_color, PARAM_FILTER);
	std::cout << " <*filter_id>\n\t: Filter the scanned modules.\n";
	std::cout << "*filter_id:\n\t0 - no filter\n\t1 - 32bit\n\t2 - 64bit\n\t3 - all (default)\n";
#endif
	print_in_color(param_color, PARAM_NO_DUMP);
	std::cout << "\t: Do not dump the modified PEs.\n";
	print_in_color(param_color, PARAM_QUIET);
	std::cout << "\t: Print only the summary. Do not create a directory with outputs.\n";

	print_in_color(hdr_color, "\nInfo: \n");
	print_in_color(param_color, PARAM_HELP);
	std::cout << "    : Print this help.\n";
	print_in_color(param_color, PARAM_VERSION);
	std::cout << " : Print version number.\n";
	std::cout << "---" << std::endl;
}

void banner()
{
	const int logo_color = 25;
	char logo[] = "\
.______    _______           _______. __   ___________    ____  _______ \n\
|   _  \\  |   ____|         /       ||  | |   ____\\   \\  /   / |   ____|\n\
|  |_)  | |  |__    ______ |   (----`|  | |  |__   \\   \\/   /  |  |__   \n\
|   ___/  |   __|  |______| \\   \\    |  | |   __|   \\      /   |   __|  \n\
|  |      |  |____      .----)   |   |  | |  |____   \\    /    |  |____ \n\
| _|      |_______|     |_______/    |__| |_______|   \\__/     |_______|\n\
  _        _______       _______      __   _______     __       _______ \n";

	print_in_color(logo_color, logo);
	std::cout << "version: " << VERSION;
#ifdef _WIN64
	std::cout << " (x64)" << "\n\n";
#else
	std::cout << " (x86)" << "\n\n";
#endif
	std::cout << "~ from hasherezade with love ~\n";
	std::cout << "Detects inline hooks and other in-memory PE modifications\n---\n";
	print_help();
}

void print_report(const t_report report, const t_params args)
{
	std::string report_str = report_to_string(report);
	//summary:
	std::cout << report_str;
	size_t total_modified = report.hooked + report.replaced + report.suspicious;
	if (!args.quiet && total_modified > 0) {
		std::string directory = make_dir_name(args.pid);
		std::cout << "\nDumps saved to the directory: " << directory << std::endl;
	}
	std::cout << "---" << std::endl;
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		banner();
		system("pause");
		return 0;
	}
	//---
	bool info_req = false;
	t_params args = { 0 };
	args.filter = LIST_MODULES_ALL;

	//Parse parameters
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], PARAM_HELP) || !strcmp(argv[i], PARAM_HELP2)) {
			print_help();
			info_req = true;
		}
		else if (!strcmp(argv[i], PARAM_IMP_REC)) {
			args.imp_rec = true;
		}
		else if (!strcmp(argv[i], PARAM_NO_DUMP)) {
			args.no_dump = true;
		} 
		else if (!strcmp(argv[i], PARAM_FILTER) && i < argc) {
			args.filter = atoi(argv[i + 1]);
			if (args.filter > LIST_MODULES_ALL) {
				args.filter = LIST_MODULES_ALL;
			}
			i++;
		}
		else if (!strcmp(argv[i], PARAM_PID) && i < argc) {
			args.pid = atoi(argv[i + 1]);
			++i;
		}
		else if (!strcmp(argv[i], PARAM_VERSION)) {
			std::cout << VERSION << std::endl;
			info_req = true;
		}
		else if (!strcmp(argv[i], PARAM_QUIET)) {
			std::cout << VERSION << std::endl;
			args.quiet = true;
		}
	}
	//if didn't received PID by explicit parameter, try to parse the first param of the app
	if (args.pid == 0) {
		if (info_req) {
#ifdef _DEBUG
			system("pause");
#endif
			return 0; // info requested, pid not given. finish.
		}
		if (argc >= 2) args.pid = atoi(argv[1]);
		if (args.pid == 0) {
			print_help();
			return 0;
		}
	}
	//---
	std::cout << "PID: " << args.pid << std::endl;
	std::cout << "Module filter: " << args.filter << std::endl;
	t_report report = check_modules_in_process(args);
	print_report(report, args);
#ifdef _DEBUG
	system("pause");
#endif
	return 0;
}
