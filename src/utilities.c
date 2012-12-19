// utilities.c
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above
//   copyright notice, this list of conditions and the following disclaimer
//   in the documentation and/or other materials provided with the
//   distribution.
// * Neither the name of SecureState Consulting nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#include "utilities.h"

int util_dir_exists(const char *dir_path) {
	struct stat st;

	if (stat(dir_path, &st) == 0) {
		return 1;
	}
	return 0;
}

int util_dir_create_if_not_exists(const char *dir_path) {
	if (util_dir_exists(dir_path) == 0) {
		return mkdir(dir_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	}
	return 0;
}

void util_str_replace(char *string, char *old, char *new) {
	char *pos = string;

	while (*pos != '\0') {
		if (*pos == *old) {
			*pos = *new;
		}
		pos++;
	}
	return;
}

void util_str_to_lower(char *string) {
	char *pos = string;

	while (*pos != '\0') {
		if ((*pos < 91) && (*pos > 64)) {
			*pos = (*pos + 32);
		}
		pos++;
	}
	return;
}

void util_str_to_upper(char *string) {
	char *pos = string;

	while (*pos != '\0') {
		if ((*pos < 123) && (*pos > 96)) {
			*pos = (*pos - 32);
		}
		pos++;
	}
	return;
}
