#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>

#include "format_data.h"

struct hrv {
	int sign;
	int mantissa;
	uint8_t value;
};

static void process_output_array_header(FILE *out, uint8_t *buffer)
{
	/* Only output leading newline for records after the first */
	static int t = 0;

	if(t != 0)
		fputc('\n', out);
	else
		t = 1;
	
	fprintf(out, "%d", ((buffer[0] & 0x3) << 8) | buffer[1]);
}

static void process_low_res_data(FILE *out, uint8_t *buffer)
{
	int sign; 
	int mantissa = (buffer[0] & 0x60) >> 5;
	int base;
	float value;
	
	if(buffer[0] & 0x80)
		sign = -1;
	else
		sign = 1;
			
	base = sign * (((buffer[0] & 0x1f) << 8) | buffer[1]);
	
	if(mantissa) {
		switch(mantissa) {
			case 1:
				value = base * 0.1;
				fprintf(out, ",%0.1f", value);
				break;
			case 2:
				value = base * 0.01;
				fprintf(out, ",%0.2f", value);
				break;
			case 3:
				value = base * 0.001;
				fprintf(out, ",%0.3f", value);
				break;
		}
	} else
		fprintf(out, ",%d", base);
}

static void process_hi_res_first_location(FILE *out, uint8_t *buffer, struct hrv *v)
{
	if(buffer[0] & 0x40)
		v->sign = -1;
	else
		v->sign = 1;

	if(buffer[0] & 0x80)
		v->mantissa = 1;
	else
		v->mantissa = 0;

	v->mantissa |= ((buffer[0] & 3) << 1);
	v->value = buffer[1];
}

static void process_hi_res_second_location(FILE *out, uint8_t *buffer, struct hrv *v)
{
	int base;
	float value;

	if(buffer[0] & 1)
		base = 0x10000;
	else
		base = 0;

	base |= (v->value << 8) | buffer[1];
	base *= v->sign;

	if(v->mantissa) {
		switch(v->mantissa) {
			case 1:
				value = base * 0.1;
				fprintf(out, ",%0.1f", value);
				break;
			case 2:
				value = base * 0.01;
				fprintf(out, ",%0.2f", value);
				break;
			case 3:
				value = base * 0.001;
				fprintf(out, ",%0.3f", value);
				break;
			case 4:
				value = base * 0.0001;
				fprintf(out, ",%0.4f", value);
				break;
			case 5:
				value = base * 0.00001;
				fprintf(out, ",%0.5f", value);
		}

	} else
		fprintf(out, ",%d", base);
}

void process_data(FILE *out, uint8_t *buffer, size_t len)
{
	int i, l;
	uint8_t b;
	static struct hrv v;

	for(i = 0, l = len / 2; i < l; i++) {
		b = buffer[i * 2];

		if((b & 0x1c) == 0x1c) {
			if((b & 0xfc) == 0xfc)
				process_output_array_header(out, buffer + i * 2);
			else if((b & 0x3c) == 0x1c)
				process_hi_res_first_location(out, buffer + i * 2, &v);
			else if((b & 0xfc) == 0x3c)
				process_hi_res_second_location(out, buffer + i * 2, &v);
		} else
			process_low_res_data(out, buffer + i * 2);
	}
}
