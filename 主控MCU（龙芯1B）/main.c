#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "bsp.h"
#include "stdio.h"
#include "ns16550.h"
#include "string.h"
#include "ls1x_i2c_bus.h"
#include "i2c/ads1015.h"
#include "ls1b_gpio.h"

#define WATER_IN 41
#define WATER_OUT 40

#define AT_TEST "AT\r\n"                                                        // 检测模块是否正常
#define AT_REQUEST_IP "AT+MIPCALL=1\r\n"                                        // 请求IP //
#define AT_SET_USER "AT+MQTTUSER=1,\"user1\",\"123456\"\r\n"                    // 设置mqtt账户
#define AT_CONNECT_MQTT "AT+MQTTOPEN=1,\"111.229.123.25\",1883,0,60\r\n"        //建立mqtt连接 //
#define AT_PUBLISH "AT+MQTTPUB=1,\"data1\",0,1,\"1=1\"\r\n"                     // 向主题发布消息
#define AT_CLOSE_MQTT "AT+MQTTCLOSE=1\r\n"                                      // 关闭mqtt连接
#define OK_RESPONSE "OK"                                                        // 状态

TaskHandle_t uart_stm32_task_handle;
TaskHandle_t water_task_handle;
TaskHandle_t time_task_handle;

unsigned short adc1=0;
float in_v1;
int rx_data_time;

// 与串口屏通信，发送温度值
void send_temp_value(double temp) {
    // 存放待发送数据
    unsigned char buf[64] = {0};
    // 修改串口屏温度显示值
    sprintf(buf, "temp.txt=\"%.2f\"", temp);
    // 将上述字符串转换为串口屏通信协议
    unsigned int buf_str_len = strlen(buf);
    int i = buf_str_len;
    for (; i < buf_str_len + 3; i++) {
        buf[i] = 0xff;
    }
    // 给串口屏发送数据
    ls1x_uart_write(devUART4, buf, buf_str_len + 3, NULL);
}
// 与串口屏通信，发送PH值
void send_ph_value(double temp) {
    unsigned char buf[64] = {0};
    sprintf(buf, "ph.txt=\"%.2f\"", temp);
    unsigned int buf_str_len = strlen(buf);
    int i = buf_str_len;
    for (; i < buf_str_len + 3; i++) {
        buf[i] = 0xff;
    }
    ls1x_uart_write(devUART4, buf, buf_str_len + 3, NULL);
}
// 与串口屏通信，发送浑浊度值
void send_turb_value(double temp) {
    unsigned char buf[64] = {0};
    sprintf(buf, "turb.txt=\"%.2f\"", temp);
    unsigned int buf_str_len = strlen(buf);
    int i = buf_str_len;
    for (; i < buf_str_len + 3; i++) {
        buf[i] = 0xff;
    }
    ls1x_uart_write(devUART4, buf, buf_str_len + 3, NULL);
}

void send_add_value(double temp) {
    unsigned char buf[64] = {0};
    sprintf(buf, "add s0.id,0,%d", (int)temp);
    unsigned int buf_str_len = strlen(buf);
    int i = buf_str_len;
    for (; i < buf_str_len + 3; i++) {
        buf[i] = 0xff;
    }
    ls1x_uart_write(devUART4, buf, buf_str_len + 3, NULL);
}

static void time_task(void *arg) {
    for(;;) {
        // 开始执行
        printf("beging");
        vTaskResume(uart_stm32_task_handle);
        vTaskResume(water_task_handle);
        vTaskDelay(rx_data_time * 1000 * 3600); // 执行的时间
        vTaskSuspend(uart_stm32_task_handle);
        vTaskSuspend(water_task_handle);
    }
}

static void uart_screen_task(void *arg)
{
    printf("\r\ntask2() function.\r\n");
    char rx_buff[100];
    for ( ; ; )
    {
        // 接收命令：屏幕
        if(ls1x_uart_read(devUART4,rx_buff,100,NULL) > 0) {
            //int rx_data_time;   // 执行的时间
            int rx_data_order;  // 0是停止1是开始
            sscanf(rx_buff,"%d=%d",&rx_data_order,&rx_data_time);
            if(rx_data_order == 1) {
                printf("begin:%d",rx_data_time);
                vTaskResume(uart_stm32_task_handle);
                if(time_task_handle == NULL){
                    xTaskCreate(time_task, // 处理任务调度
                                "timetask",
                                2048,
				                NULL,
                                31,
				                &time_task_handle);
                }
            }
            if(rx_data_order == 0) {
                printf("stop");
                 // 停止执行
                gpio_write(WATER_IN,0);
                gpio_write(WATER_OUT,0);
                vTaskSuspend(uart_stm32_task_handle);
                vTaskSuspend(water_task_handle);
                if(time_task_handle != NULL){
                    vTaskDelete(time_task_handle);
                }
            }
        }
        memset(rx_buff,0,sizeof(rx_buff)); // 清除缓存
        vTaskDelay(500);
        memset(rx_buff,0,sizeof(rx_buff)); // 清除缓存
        vTaskDelay(500);
    }
}





static void uart_stm32_task(void *arg)
{
    
    // 连接mqtt服务器
    taskENTER_CRITICAL();   // 保护串口发送
    printk("ip");
    //vTaskDelay(10000);
    ls1x_uart_write(devUART1,AT_REQUEST_IP,sizeof(AT_REQUEST_IP),NULL);
    printk("ok");
    vTaskDelay(3000);
    ls1x_uart_write(devUART1,AT_SET_USER,sizeof(AT_SET_USER),NULL);
    printk("userok");
    vTaskDelay(3000);
    ls1x_uart_write(devUART1,AT_CONNECT_MQTT,sizeof(AT_CONNECT_MQTT),NULL);
    printk("mqttok");
    taskEXIT_CRITICAL();    // 退出保护

    vTaskSuspend(NULL); //停止任务等待运行
    for ( ; ; )
    {
        float temp_value;
        float ph_value;
        float turb_value;
        char temp_value_4g[50];
        char ph_value_4g[50];
        char turb_value_4g[50];
        float rx_buf1;
        float rx_buf2;
        float rx_buf3;
        char rx_buff[50];

        // 1.接收stm32数据
        if(ls1x_uart_read(devUART1,rx_buff,100,NULL) > 0) {
            printf("ok");
            sscanf(rx_buff,"0=%f|1=%f|2=%f|\n",&rx_buf1,&rx_buf2,&rx_buf3);
        }


        // 2.处理数据
        // PH传感器算法
        ph_value = 7.0f - (ph_voltage - 1.65f) / 0.18f; // 假设中性电压为1.65V，斜率为0.18V/pH
        // 温度传感器 (PT100) 算法
        float temp_voltage = rx_buf2 * (3.3f / 4095.0f);
        temp_value = (temp_voltage - 0.5f) * 100.0f; // 假设0.5V对应0°C，10mV/°C
        // 浑浊度传感器算法
        float turb_voltage = rx_buf3 * (3.3f / 4095.0f);
        turb_value = turb_voltage * 3000.0f; // 假设3.3V对应3000 NTU
        
        // 3.发送数据
        vTaskDelay(100);
        taskENTER_CRITICAL();
        send_temp_value(temp_value);
        ls1x_uart_write(devUART1,ph_value_4g,strlen(ph_value_4g),NULL);
        vTaskDelay(100);
        send_ph_value(ph_value);
        ls1x_uart_write(devUART1,temp_value_4g,strlen(temp_value_4g),NULL);
        vTaskDelay(100);
        send_turb_value(turb_value);
        ls1x_uart_write(devUART1,turb_value_4g,strlen(turb_value_4g),NULL);
        send_add_value(turb_value*50 + 40);
        taskEXIT_CRITICAL();
        vTaskDelay(500);
        
        
    }
}

static void water_task(void *arg) {
    gpio_write(WATER_OUT,0);
    gpio_write(WATER_IN,0);

    vTaskSuspend(NULL); //停止任务等待运行
    for(;;) {
        //
        gpio_write(WATER_OUT,0);    
        gpio_write(WATER_IN,1);     // 1. 进水
        vTaskDelay(25000);           // 2.进水时长：根据实际情况调整
        gpio_write(WATER_IN,0);     // 3.关水
        vTaskDelay(5000);           // 4.等待一定时长，读取数据
        
        //
        gpio_write(WATER_OUT,1);    // 1.出水
        vTaskDelay(60000);           // 2.进水时长：根据实际情况调整
        gpio_write(WATER_OUT,0);    // 3.关水
        
        vTaskDelay(5000); // 每次延时时间
    }
}



int main(void)
{
    printk("\r\nmain() function.\r\n");
    ls1x_drv_init();            		/* Initialize device drivers */
    install_3th_libraries();      		/* Install 3th libraies */
    ls1x_uart_init(devUART4,NULL); //初始化串口:串口屏幕
    ls1x_uart_open(devUART4,NULL);
    ls1x_uart_init(devUART1,NULL); //初始化串口：esp32和4g
    ls1x_uart_open(devUART1,NULL);
    gpio_enable(WATER_IN, DIR_OUT); //初始化GPIO:2个电机控制
    gpio_enable(WATER_OUT, DIR_OUT);
    gpio_write(WATER_IN,0);
    gpio_write(WATER_OUT,0);
    gpio_enable(54, DIR_OUT);
    gpio_write(54,1);


    
    xTaskCreate(uart_screen_task, // 处理任务调度
                "uartscreentask",
                2048,
				NULL,
                31,
				NULL);
    xTaskCreate(uart_stm32_task, // 接收esp32串口并发送到屏幕和4g模块
                "uartstm32task",
                2048,
				NULL,
                31,
				&uart_stm32_task_handle);
    xTaskCreate(water_task, // 水泵
                "uartwatertask",
                2048,
				NULL,
                31,
				&water_task_handle);


	vTaskStartScheduler();
	for ( ; ; )
	{
        ;
	}
    return 0;
}

