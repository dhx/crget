#include <stdio.h>

void draw_bar(int cur, int max)
{
	int p = (cur * 100) / max, b = p / 5, i;
	
	if(cur != max) 
		printf("\r %d%%\t[", p);
	else
		printf("\r100%%\t[");

	for(i = 0; i < 20; i++) {
		if(b >= i)
			putchar('*');
		else
			putchar(' ');
	}

	printf("]\t%d / %d ", cur, max);

	fflush(stdout);
}

int main()
{
	int i;

	for(i = 0; i <= 31; i++) {
		draw_bar(i, 31);
		sleep(1);
	}
}
