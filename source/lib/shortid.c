#include <string.h>
#include <math.h>
#include <stdlib.h>

const char* ALPHABET = "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

unsigned long ipow(double a, double b) {
	return floor(pow(a, b));
}

unsigned long decode(char* value, int pad) {
	long len = (long) strlen(value) - 1;
	unsigned long result = 0;
	for (long t = len; t >= 0; t--) {
		unsigned long bcp = (unsigned long) ipow((double) strlen(ALPHABET), (double) (len - t));
		result += (unsigned long) (value[t] * bcp);
	}

	if (pad > 0) {
		result -= (unsigned long) ipow((double) strlen(ALPHABET), pad);
	}

	return result;
}

char* encode(unsigned long value, int pad) {
	char* result = (char*) calloc(256, sizeof(char));

	if (pad > 0) {
		value += (unsigned long) ipow((double) strlen(ALPHABET), pad);
	}
	long lg = (int) (log((double) value) / log((double) strlen(ALPHABET)));
	for (long t = (value != 0 ? lg : 0); t >= 0; t--) {
		unsigned long bcp = ipow((double) strlen(ALPHABET),(double) t);
		unsigned long a =
				((unsigned long) floor((double) value / (double) bcp)) % (unsigned long) strlen(ALPHABET);
		long len = (long) strlen(result);
		result[len] = ALPHABET[(int) a];
		result[len + 1] = '\0';
		value = value - (a * bcp);
	}
	return result;
}
