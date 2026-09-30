/*
 * Copyright (c) 2024 ~ 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_adb.h"
#include "csh.h"
#include "chry_ringbuffer.h"

#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

#ifndef CHERRYSH_TASK_REPL_PRIORITY
#define CHERRYSH_TASK_REPL_PRIORITY (2)
#endif

#ifndef CHERRYSH_TASK_EXEC_PRIORITY
#define CHERRYSH_TASK_EXEC_PRIORITY (1)
#endif

#ifndef CHERRYSH_TASK_REPL_STACK_SIZE
#define CHERRYSH_TASK_REPL_STACK_SIZE 1024U
#endif

#ifndef CHERRYSH_TASK_EXEC_STACK_SIZE
#define CHERRYSH_TASK_EXEC_STACK_SIZE 1024U
#endif

static chry_shell_t csh;
static volatile bool login = false;
static chry_ringbuffer_t shell_rb;
static uint8_t mempool[1024];

static TaskHandle_t task_hdl_repl = NULL;
static TaskHandle_t task_hdl_exec = NULL;

static EventGroupHandle_t event_hdl;
static StaticEventGroup_t event_grp;

static int shell_init(bool need_login);

static void adb_shell_on_open(uint32_t remoteid)
{
    USB_LOG_INFO("adb shell open, remoteid:%u\r\n", (unsigned)remoteid);
}

static void adb_shell_on_close(uint32_t remoteid)
{
    USB_LOG_INFO("adb shell close, remoteid:%u\r\n", (unsigned)remoteid);
}

static void adb_shell_on_write(uint32_t remoteid, const uint8_t *data, uint32_t len)
{
    chry_ringbuffer_write(&shell_rb, (uint8_t *)data, len);
    xEventGroupSetBits(event_hdl, 0x10);
}

static const struct adb_service adb_shell_service = {
    .name = "shell:",
    .localid = ADB_LOCALID_SHELL,
    .on_open = adb_shell_on_open,
    .on_close = adb_shell_on_close,
    .on_write = adb_shell_on_write,
};

void usbd_adb_shell_init(void)
{
    if (usbd_adb_service_register(&adb_shell_service) != 0) {
        USB_LOG_ERR("adb shell: register failed\r\n");
    }
    shell_init(false);
}

static uint16_t csh_sput_cb(chry_readline_t *rl, const void *data, uint16_t size)
{
    (void)rl;

    usbd_adb_write(ADB_LOCALID_SHELL, data, size);

    return size;
}

static uint16_t csh_sget_cb(chry_readline_t *rl, void *data, uint16_t size)
{
    (void)rl;
    return chry_ringbuffer_read(&shell_rb, data, size);
}

static void wait_char(void)
{
    EventBits_t event;
wait:
    /* In order to lock the log from being disrupted , wait for REPL task execution to complete */
    event = xEventGroupWaitBits(event_hdl, (0x10 | 0x01 | 0x04), pdTRUE, pdFALSE, portMAX_DELAY);
    if ((event & 0x10) == 0) {
        if (event & 0x01) {
            chry_readline_erase_line(&csh.rl);
            xEventGroupSetBits(event_hdl, 0x02);
        }
        if (event & 0x04) {
            chry_readline_edit_refresh(&csh.rl);
            xEventGroupSetBits(event_hdl, 0x08);
        }

        goto wait;
    }
}

static void task_repl(void *param)
{
    (void)param;
    int ret;
    volatile uint8_t *pexec = (void *)&csh.exec;

    for (;;) {
    restart:
        if (login) {
            goto repl;
        } else {
        }

        ret = csh_login(&csh);
        if (ret == 0) {
            login = true;
        } else if (ret == 1) {
            /*!< no enough char */
            wait_char();
            continue;
        } else {
            continue;
        }

    repl:
        ret = chry_shell_task_repl(&csh);

        if (ret == -1) {
            /*!< error */
            goto restart;
        } else if (ret == 1) {
            /*!< no enough char */
            wait_char();
        } else {
            /*!< restart */
        }

        /*!< check flag */
        if (*pexec == CSH_STATUS_EXEC_DONE) {
            *pexec = CSH_STATUS_EXEC_IDLE;
            chry_readline_auto_refresh(&csh.rl, true);
            chry_readline_ignore(&csh.rl, false);
            chry_readline_edit_refresh(&csh.rl);
        }

        if (login == false) {
            chry_readline_erase_line(&csh.rl);
            csh.rl.noblock = false;
        }
    }
}

static void task_exec(void *param)
{
    (void)param;

    /*!< execute shell command */
    chry_shell_task_exec(&csh);

    /*!< notify REPL task execute done */
    xEventGroupSetBits(event_hdl, 0x10);

    /*!< wait for REPL task delete */
    vTaskSuspend(NULL);
}

int chry_shell_port_create_context(chry_shell_t *csh, int argc, const char **argv)
{
    volatile TaskHandle_t *p_task_hdl_exec = (void *)&task_hdl_exec;
    (void)csh;
    (void)argc;
    (void)argv;

    if (*p_task_hdl_exec != NULL) {
        vTaskDelete(*p_task_hdl_exec);
    }

    *p_task_hdl_exec = NULL;
    if (xTaskCreate(task_exec, "task_exec", CHERRYSH_TASK_EXEC_STACK_SIZE, NULL, CHERRYSH_TASK_EXEC_PRIORITY, (TaskHandle_t *)p_task_hdl_exec) != pdPASS) {
        return -1;
    }
    return 0;
}

void chry_shell_port_default_handler(chry_shell_t *csh, int sig)
{
    volatile uint8_t *pexec = (void *)&csh->exec;
    volatile TaskHandle_t *p_task_hdl_exec = (void *)&task_hdl_exec;

    switch (sig) {
        case CSH_SIGINT:
        case CSH_SIGQUIT:
        case CSH_SIGKILL:
        case CSH_SIGTERM:
            break;
        default:
            return;
    }

    /*!< force delete task */
    if (*p_task_hdl_exec != NULL) {
        vTaskDelete(task_hdl_exec);
        *p_task_hdl_exec = NULL;
    }

    switch (sig) {
        case CSH_SIGINT:
            csh->rl.sput(&csh->rl, "^SIGINT" CONFIG_CSH_NEWLINE, sizeof("^SIGINT" CONFIG_CSH_NEWLINE) - 1);
            break;
        case CSH_SIGQUIT:
            csh->rl.sput(&csh->rl, "^SIGQUIT" CONFIG_CSH_NEWLINE, sizeof("^SIGQUIT" CONFIG_CSH_NEWLINE) - 1);
            break;
        case CSH_SIGKILL:
            csh->rl.sput(&csh->rl, "^SIGKILL" CONFIG_CSH_NEWLINE, sizeof("^SIGKILL" CONFIG_CSH_NEWLINE) - 1);
            break;
        case CSH_SIGTERM:
            csh->rl.sput(&csh->rl, "^SIGTERM" CONFIG_CSH_NEWLINE, sizeof("^SIGTERM" CONFIG_CSH_NEWLINE) - 1);
            break;
        default:
            return;
    }

    *pexec = CSH_STATUS_EXEC_IDLE;
    chry_readline_auto_refresh(&csh->rl, true);
    chry_readline_ignore(&csh->rl, false);
    chry_readline_edit_refresh(&csh->rl);
}

static int shell_init(bool need_login)
{
    chry_shell_init_t csh_init = {0};

    if (chry_ringbuffer_init(&shell_rb, mempool, sizeof(mempool))) {
        return -1;
    }

    if (need_login) {
        login = false;
    } else {
        login = true;
    }

    /*!< I/O callback */
    csh_init.sput = csh_sput_cb;
    csh_init.sget = csh_sget_cb;

#if defined(CONFIG_CSH_SYMTAB) && CONFIG_CSH_SYMTAB
#if defined(__GNUC__)
    extern const int __fsymtab_start;
    extern const int __fsymtab_end;
    extern const int __vsymtab_start;
    extern const int __vsymtab_end;

    /*!< get table from ld symbol */
    csh_init.command_table_beg = &__fsymtab_start;
    csh_init.command_table_end = &__fsymtab_end;
    csh_init.variable_table_beg = &__vsymtab_start;
    csh_init.variable_table_end = &__vsymtab_end;
#elif defined(__ICCARM__) || defined(__ICCRX__) || defined(__ICCRISCV__)
#pragma section="FSymTab"
#pragma section="VSymTab"
    csh_init.command_table_beg = __section_begin("FSymTab");
    csh_init.command_table_end = __section_end("FSymTab");
    csh_init.variable_table_beg = __section_begin("VSymTab");
    csh_init.variable_table_end = __section_end("VSymTab");
#endif
#endif

#if defined(CONFIG_CSH_PROMPTEDIT) && CONFIG_CSH_PROMPTEDIT
    static char csh_prompt_buffer[128];

    /*!< set prompt buffer */
    csh_init.prompt_buffer = csh_prompt_buffer;
    csh_init.prompt_buffer_size = sizeof(csh_prompt_buffer);
#endif

#if defined(CONFIG_CSH_HISTORY) && CONFIG_CSH_HISTORY
    static char csh_history_buffer[128];

    /*!< set history buffer */
    csh_init.history_buffer = csh_history_buffer;
    csh_init.history_buffer_size = sizeof(csh_history_buffer);
#endif

#if defined(CONFIG_CSH_LNBUFF_STATIC) && CONFIG_CSH_LNBUFF_STATIC
    static char csh_line_buffer[128];

    /*!< set linebuffer */
    csh_init.line_buffer = csh_line_buffer;
    csh_init.line_buffer_size = sizeof(csh_line_buffer);
#endif

    csh_init.uid = 0;
    csh_init.user[0] = "cherry";

    /*!< The port hash function is required,
         and the strcmp attribute is used weakly by default,
         int chry_shell_port_hash_strcmp(const char *hash, const char *str); */
    csh_init.hash[0] = "12345678"; /*!< If there is no password, set to NULL */
    csh_init.host = BOARD_NAME;
    csh_init.user_data = NULL;

    int ret = chry_shell_init(&csh, &csh_init);
    if (ret) {
        return -1;
    }

    task_hdl_exec = NULL;
    event_hdl = xEventGroupCreateStatic(&event_grp);
    task_hdl_repl = NULL;
    if (xTaskCreate(task_repl, "task_repl", CHERRYSH_TASK_REPL_STACK_SIZE, NULL, CHERRYSH_TASK_REPL_PRIORITY, (TaskHandle_t *)&task_hdl_repl) != pdPASS) {
        return -1;
    }

    return 0;
}

void shell_lock(void)
{
    xEventGroupSetBits(event_hdl, 0x01);
    xEventGroupWaitBits(event_hdl, 0x02, pdTRUE, pdTRUE, portMAX_DELAY);
}

void shell_unlock(void)
{
    xEventGroupSetBits(event_hdl, 0x04);
    xEventGroupWaitBits(event_hdl, 0x08, pdTRUE, pdTRUE, portMAX_DELAY);
}

static int csh_exit(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    usbd_adb_close(ADB_LOCALID_SHELL);
    return 0;
}
CSH_SCMD_EXPORT_ALIAS(csh_exit, exit, );

#define __ENV_PATH "/sbin:/bin"
const char ENV_PATH[] = __ENV_PATH;
CSH_RVAR_EXPORT(ENV_PATH, PATH, sizeof(__ENV_PATH));

#define __ENV_ZERO ""
const char ENV_ZERO[] = __ENV_ZERO;
CSH_RVAR_EXPORT(ENV_ZERO, ZERO, sizeof(__ENV_ZERO));
