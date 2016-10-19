#include "config.h"
#include "if_impl.h"
#include "linkedlist.h"
#include "packet_plugin.h"
#include "packet_builder.h"
#include "logging.h"
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <signal.h>

void recv_frame_handler(ETH_EAP_FRAME *frame) {
    printf("We had one! %d %d\n", frame->actual_len,frame->buffer_len);
    packet_plugin_on_frame_received(frame);
}

/*
 * Initialize the settings.
 * Note: Override values in config file with cmdline.
 */
int init_program_config(int argc, char* argv[]) {
    PROG_CONFIG* cfg;
    if (IS_FAIL(parse_cmdline_conf_file(argc, argv))) {
        PR_ERR("配置文件路径解析出错");
        goto err;
    }
    
    cfg = get_program_config();
    if (IS_FAIL(parse_config_file(cfg->conffile))) {
        PR_ERR("配置文件内容解析出错");
        goto err;
    }
    if (IS_FAIL(parse_cmdline_opts(argc, argv))) {
        PR_ERR("命令行参数解析出错");
        goto err;
    }

    return SUCCESS;
err:
    return FAILURE;
}

/*
 * Initialize the settings. Called after plugins being selected.
 * Note: Override values in config file with cmdline.
 */
int init_plugin_config(int argc, char* argv[]) {
    PROG_CONFIG *cfg = get_program_config();
    if (IS_FAIL(packet_plugin_process_config_file(cfg->conffile))) {
        PR_ERR("插件配置文件内容解析出错");
        goto err;
    }
    if (IS_FAIL(packet_plugin_process_cmdline_opts(argc, argv))) {
        PR_ERR("插件配置文件内容解析出错");
        goto err;
    }
    return SUCCESS;
err:
    return FAILURE;
}

static void packet_plugin_list_traverse(void* name, void* unused) {
    select_packet_plugin((const char* )name);
}

/*
 * Do all the initialization tasks
 */
int init_env(int argc, char* argv[]) {
    PROG_CONFIG* cfg = get_program_config();
    
    set_log_destination(LOG_TO_CONSOLE);
    
    init_if_impl_list();
    init_packet_plugin_list();
    
    if (IS_FAIL(init_program_config(argc, argv))) {
        PR_ERR("参数初始化错误");
        return FAILURE;
    }

    list_traverse(cfg->packet_plugin_list, packet_plugin_list_traverse, NULL);

    if (IS_FAIL(init_plugin_config(argc, argv))) {
        PR_ERR("插件初始化错误");
        return FAILURE;
    }
    return SUCCESS;
}

int init_if() {
    PROG_CONFIG* cfg = get_program_config();
    IF_IMPL* if_impl;
    
    if (IS_FAIL(select_if_impl(cfg->if_impl))) {
        PR_ERR("网络驱动插件启用失败，请检查插件名称是否拼写正确");
        return FAILURE;
    }
    
    if_impl = get_if_impl();
    if (IS_FAIL(if_impl->set_ifname(if_impl,cfg->ifname))) {
        PR_ERR("设置接口名称失败");
        return FAILURE;
    }
    
    if (IS_FAIL(if_impl->setup_capture_params(if_impl, htons(ETH_P_PAE), FALSE))) {
        PR_ERR("设置捕获参数失败");
        return FAILURE;
    }
    
    if_impl->set_frame_handler(if_impl, recv_frame_handler);
    
    return SUCCESS;
}

void exit_handler() {
    free_config();
    free_if_impl();
    packet_builder_destroy(packet_builder_get());
    packet_plugin_destroy();
    PR_INFO("MiniEAP 已退出");
    close_log();
};

void signal_handler(int signal) {
    exit(0);
}

/*
 * Detailed errors are printed where they happen, not here ...
 */
int main(int argc, char* argv[]) {
    IF_IMPL* if_impl;
    
    atexit(exit_handler);
	signal(SIGALRM, signal_handler);
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	
    if (IS_FAIL(init_env(argc, argv))) {
        return FAILURE;
    }

    if (IS_FAIL(init_if())) {
        return FAILURE;
    }
    
    if_impl = get_if_impl();
    if_impl->start_capture(if_impl);
    
    /*
    if_impl->obtain_mac(if_impl, mac);
    printf("%02x%02x%02x%02x%02x%02x\n", mac[0], mac[1],mac[2],mac[3],mac[4],mac[5]);
*/
    return 0;
}

