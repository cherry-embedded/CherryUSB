#ifndef __GETOPT_H__
#define __GETOPT_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parameters needed to parse the command line
 *
 */
typedef struct getopt_env {
    char *optarg;    /*!< if the option accepts parameters, then optarg point to the option parameter*/
    int optind;      /*!< current index of argv*/
    int opterr;      /*!< non-zero enable error message output, while 0,no error message output*/
    int optopt;      /*!< contain unrecognized option character*/
    int __optpos;
} getopt_env_t;

/**
 * @brief Initialize struct getopt_env
 *
 * @param env pointer to struct getopt_env
 * @param opterr set error message output method
 *
 * @return
 *     -  0: success
 *     - -1: fail
 */
int utils_getopt_init(getopt_env_t *env, int opterr);

/**
 * @brief Parses the command-line arguments
 *
 * @param env pointer to struct getopt_env
 * @param argc the argument count
 * @param argv the argument array
 *
 * @return
 *     -  option character : an option was successfully found
 *     - -1 : all command-line options have been parsed
 *     - '?' : option character was not in optstring
 *     - ':' or '?' : If utils_getopt() encounters an option with a missing argument, then the return value depends on the first character in optstring: if it is ':', then ':' is returned; otherwise '?' is returned
 *
 * @note Example
 * @code
 *
 * #include <utils_getopt.h>
 * #include <stdio.h>
 *
 * void cmd(char *buf, int len, int argc, char **argv)
 * {
 *     int opt;
       getopt_env_t getopt_env;
       utils_getopt_init(&getopt_env, 0);
 *     //put ':' in the starting of the string so that program can distinguish between '?' and ':'
 *     while ((opt = utils_getopt(&getopt_env, argc, argv, ":if:lr")) != -1) {
 *         switch(opt)
 *         {
 *             case 'i':
 *             case 'l':
 *             case 'r':
 *                 printf("option: %c\r\n", opt);
 *                 break;
 *             case 'f':
 *                 printf("filename: %s\r\n", getopt_env.optarg);
 *                 break;
 *             case ':':
                   printf("%s: %c requires an argument\r\n", *argv, getopt_env.optopt);
 *                 break;
 *             case '?':
 *                 printf("unknow option: %c\r\n", getopt_env.optopt);
 *                 break;
 *          }
 *      }
 *      //optind is for the extra arguments which are not parsed
 *      for(; getopt_env.optind < argc; getopt_env.optind++){
 *          printf("extra arguments: %s\r\n", argv[getopt_env.optind]);
 *      }
 *
 *  }
 *  @endcode
 */
int utils_getopt(getopt_env_t *env, int argc, char *const argv[], const char *optstring);

void get_bytearray_from_string(char **params, uint8_t *result, int array_size);
void get_uint8_from_string(char **params, uint8_t *result);
void get_uint16_from_string(char **params, uint16_t *result);
void get_uint32_from_string(char **params, uint32_t *result);
void utils_parse_number(const char *str, char sep, uint8_t *buf, int buflen, int base);
void utils_parse_number_adv(const char *str, char sep, uint8_t *buf, int buflen, int base, int *count);
unsigned long long convert_arrayToU64(uint8_t *inputArray);
void convert_u64ToArray(unsigned long long inputU64, uint8_t result[8]);
void utils_memdrain8(void *src, size_t len);
void utils_memdrain16(void *src, size_t len);
void utils_memdrain32(void *src, size_t len);
void utils_memdrain64(void *src, size_t len);
void *utils_memdrain8_with_check(void *src, size_t len, uint8_t seq);
void *utils_memdrain16_with_check(void *src, size_t len, uint16_t seq);
void *utils_memdrain32_with_check(void *src, size_t len, uint32_t seq);
void *utils_memdrain64_with_check(void *src, size_t len, uint64_t seq);

#ifdef __cplusplus
}
#endif

#endif /* __GETOPT_H__ */
