#include <iostream>
#include <string>
#include <cstring>
#include <bitset>
#include <sys/mman.h>

#include "elapsed_time.h"
namespace ela = elapsed_time;

#include "amount_formatter.h"
namespace amf = amount_formatter;

using namespace std;

const size_t ONE_GIG = 1000 * 1000 * 1000;

const size_t DEFAULT_SIZE = 8 * ONE_GIG;

bool process_opts(int argc, char *argv[], size_t &size) {
	if (argc <= 1) {
		size = DEFAULT_SIZE;
	} else {
		string arg = argv[1];

		if (arg.size() == 0) {
			size = DEFAULT_SIZE;
		} else {
			char suffix = arg.at(arg.size() - 1);

			if (suffix < '0' && suffix <= '9') {
				suffix = '1';
				arg = arg.substr(0, arg.size() - 1);
			}

			size = stoi(arg);

			switch (suffix) {
			case 'e':
			case 'E':
				size *= 1000;
				/* no break */
			case 'p':
			case 'P':
				size *= 1000;
				/* no break */
			case 't':
			case 'T':
				size *= 1000;
				/* no break */
			case 'g':
			case 'G':
				size *= 1000;
				/* no break */
			case 'm':
			case 'M':
				size *= 1000;
				/* no break */
			case 'k':
			case 'K':
				size *= 1000;
				/* no break */
			case '1':
				break;

			default:
				cerr << "error: unknown unit prefix letter: " << suffix << "\n";
				size = 0;
				return true;
			}
		}
	}

	return false;
}

void set_zebra(uint64_t *mem, size_t size_uint64) {
	for (uint64_t *p = mem; p < mem + size_uint64;) {
		*(p++) = 0;
		*(p++) = (uint64_t) -1;
	}
}

inline void check_next(uint64_t *&p, const uint64_t &expected, bool &seu_found) {
	if (*p != expected) {
		string time_str = ela::format_time(ela::system_time());

		cout << time_str << " SEU found at " << p << ":\n";
		cout << time_str << "     expected: " << bitset<64>(expected) << "\n";
		cout << time_str << "     found   : " << bitset<64>(*p) << "\n" << flush;

		seu_found = true;

		*p = (uint64_t) (-1);
	}
}

bool check_and_repair_mem(uint64_t *mem, size_t size_uint64) {
	bool seu_found = false;

	for (uint64_t *p = mem; p < mem + size_uint64;) {
		check_next(p, 0, seu_found);
		p++;
		check_next(p, -1, seu_found);
		p++;
	}

	return seu_found;
}

int main(int argc, char *argv[]) {
	size_t size_byte;

	bool opts_failed = process_opts(argc, argv, size_byte);
	if (opts_failed) {
		return 1;
	}

	if ((size_byte % 16) != 0) {
		cerr << "error: size % 16 = " << (size_byte % 16)
				<< " is not supported. must line up with an even number of uint64.\n" << flush;
		return 1;
	}

	cout << ela::format_time(ela::system_time()) << " allocating " << size_byte << " bytes.\n" << flush;

	uint64_t *mem = (uint64_t*) malloc(size_byte);

	ela::elapsed_time_ns t_alloc = ela::system_time();

	if (mem == NULL) {
		cerr << ela::format_time(ela::system_time()) << " error allocating memory: ";
		cerr << strerror(errno) << " (" << errno << ")\n" << flush;

		return 1;
	}

	cout << ela::format_time(ela::system_time()) << " allocated memory at " << mem << ".\n" << flush;

	bool lock_failed = mlock(mem, size_byte);

	if (lock_failed) {
		cerr << ela::format_time(ela::system_time()) << " failed to lock memory to ram: ";
		if (errno == ENOMEM) {
			cerr << "ENOMEN: RLIMIT_MEMLOCK exceeded (" << errno << ") (this usually fails and doesn't matter.)\n"
					<< flush;
		} else {
			cerr << strerror(errno) << " (" << errno << ")\n" << flush;
		}

		// errno:
		// ENOMEM: 12
		// EPERM: 1
		// EAGAIN: 11
		// EINVAL: 22
	}

	cout << ela::format_time(ela::system_time()) << " initializing memory.\n" << flush;

	size_t size_uint64 = size_byte / 8;

	set_zebra(mem, size_uint64);

	cout << ela::format_time(ela::system_time()) << " memory preparation completed.\n" << flush;

	for (;;) {
		cout << ela::format_time(ela::system_time()) << " press [return] to check for single event upsets.\n" << flush;

		char inp;
		do {
			inp = cin.get();
		} while (inp != 10);

		cout << ela::format_time(ela::system_time()) << " checking memory for SEU.\n" << flush;

		ela::elapsed_time_ns t_check_start = ela::system_time();

		bool seu_found = check_and_repair_mem(mem, size_uint64);

		ela::elapsed_time_ns uptime = ela::system_time() - t_alloc;

		if (!seu_found) {
			cout << ela::format_time(ela::system_time()) << " no SEU found. check took "
					<< ela::dura_since(t_check_start) << ". uptime: " << ela::format_dura(uptime) << " = "
					<< ela::format_dura_s(uptime) << " sec\n" << flush;
		}
	}

	if (lock_failed) {
		munlock(mem, size_byte);
	}

	free(mem);
	mem = NULL;

	return 0;
}
