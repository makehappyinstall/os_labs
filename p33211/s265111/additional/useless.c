#include <string.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    FILE* file = fopen("useless_output.txt", "w");
    if (file == NULL) {
        printf("smth went wrong");
        return 1;
    }
    char buf[1024];
    unsigned long final_length = 0;
    char* welcome_str = "This is very useless program\n";
    final_length += strlen(welcome_str);
    strcpy(buf, welcome_str);
    for (int i = 0; i < argc; ++i) {
    	if (argv[i] != NULL) {
        	strcat(buf, argv[i]);
        	strcat(buf, "\n");
        	final_length += strlen(argv[i]) + 1;
        }
    }
    fwrite(buf, 1, final_length, file);
    printf("Args were written to the file 'useless_output.txt'\n");
    fclose(file);
    return 0;
}

