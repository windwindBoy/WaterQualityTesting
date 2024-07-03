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

#define AT_TEST "AT\r\n"                                                        // ���ģ���Ƿ�����
#define AT_REQUEST_IP "AT+MIPCALL=1\r\n"                                        // ����IP //
#define AT_SET_USER "AT+MQTTUSER=1,\"user1\",\"123456\"\r\n"                    // ����mqtt�˻�
#define AT_CONNECT_MQTT "AT+MQTTOPEN=1,\"111.229.123.25\",1883,0,60\r\n"        //����mqtt���� //
#define AT_PUBLISH "AT+MQTTPUB=1,\"data1\",0,1,\"1=1\"\r\n"                     // �����ⷢ����Ϣ
#define AT_CLOSE_MQTT "AT+MQTTCLOSE=1\r\n"                                      // �ر�mqtt����
#define OK_RESPONSE "OK"                                                        // ״̬

TaskHandle_t uart_stm32_task_handle;
TaskHandle_t water_task_handle;
TaskHandle_t time_task_handle;

unsigned short adc1=0;
float in_v1;
int rx_data_time;

// �봮����ͨ�ţ������¶�ֵ
void send_temp_value(double temp) {
    // ��Ŵ���������
    unsigned char buf[64] = {0};
    // �޸Ĵ������¶���ʾֵ
    sprintf(buf, "temp.txt=\"%.2f\"", temp);
    // �������ַ���ת��Ϊ������ͨ��Э��
    unsigned int buf_str_len = strlen(buf);
    int i = buf_str_len;
    for (; i < buf_str_len + 3; i++) {
        buf[i] = 0xff;
    }
    // ����������������
    ls1x_uart_write(devUART4, buf, buf_str_len + 3, NULL);
}
// �봮����ͨ�ţ�����PHֵ
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
// �봮����ͨ�ţ����ͻ��Ƕ�ֵ
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
        // ��ʼִ��
        printf("beging");
        vTaskResume(uart_stm32_task_handle);
        vTaskResume(water_task_handle);
        vTaskDelay(rx_data_time * 1000 * 3600); // ִ�е�ʱ��
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
        // ���������Ļ
        if(ls1x_uart_read(devUART4,rx_buff,100,NULL) > 0) {
            //int rx_data_time;   // ִ�е�ʱ��
            int rx_data_order;  // 0��ֹͣ1�ǿ�ʼ
            sscanf(rx_buff,"%d=%d",&rx_data_order,&rx_data_time);
            if(rx_data_order == 1) {
                printf("begin:%d",rx_data_time);
                vTaskResume(uart_stm32_task_handle);
                if(time_task_handle == NULL){
                    xTaskCreate(time_task, // �����������
                                "timetask",
                                2048,
				                NULL,
                                31,
				                &time_task_handle);
                }
            }
            if(rx_data_order == 0) {
                printf("stop");
                 // ִֹͣ��
                gpio_write(WATER_IN,0);
                gpio_write(WATER_OUT,0);
                vTaskSuspend(uart_stm32_task_handle);
                vTaskSuspend(water_task_handle);
                if(time_task_handle != NULL){
                    vTaskDelete(time_task_handle);
                }
            }
        }
        memset(rx_buff,0,sizeof(rx_buff)); // �������
        vTaskDelay(500);
        memset(rx_buff,0,sizeof(rx_buff)); // �������
        vTaskDelay(500);
    }
}





static void uart_stm32_task(void *arg)
{
    
    // ����mqtt������
    taskENTER_CRITICAL();   // �������ڷ���
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
    taskEXIT_CRITICAL();    // �˳�����

    vTaskSuspend(NULL); //ֹͣ����ȴ�����
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

        // 1.����stm32����
        if(ls1x_uart_read(devUART1,rx_buff,100,NULL) > 0) {
            printf("ok");
            sscanf(rx_buff,"0=%f|1=%f|2=%f|\n",&rx_buf1,&rx_buf2,&rx_buf3);
        }


        // 2.��������
        // PH�������㷨
        ph_value = 7.0f - (ph_voltage - 1.65f) / 0.18f; // �������Ե�ѹΪ1.65V��б��Ϊ0.18V/pH
        // �¶ȴ����� (PT100) �㷨
        float temp_voltage = rx_buf2 * (3.3f / 4095.0f);
        temp_value = (temp_voltage - 0.5f) * 100.0f; // ����0.5V��Ӧ0��C��10mV/��C
        // ���Ƕȴ������㷨
        float turb_voltage = rx_buf3 * (3.3f / 4095.0f);
        turb_value = turb_voltage * 3000.0f; // ����3.3V��Ӧ3000 NTU
        
        // 3.��������
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

    vTaskSuspend(NULL); //ֹͣ����ȴ�����
    for(;;) {
        //
        gpio_write(WATER_OUT,0);    
        gpio_write(WATER_IN,1);     // 1. ��ˮ
        vTaskDelay(25000);           // 2.��ˮʱ��������ʵ���������
        gpio_write(WATER_IN,0);     // 3.��ˮ
        vTaskDelay(5000);           // 4.�ȴ�һ��ʱ������ȡ����
        
        //
        gpio_write(WATER_OUT,1);    // 1.��ˮ
        vTaskDelay(60000);           // 2.��ˮʱ��������ʵ���������
        gpio_write(WATER_OUT,0);    // 3.��ˮ
        
        vTaskDelay(5000); // ÿ����ʱʱ��
    }
}



int main(void)
{
    printk("\r\nmain() function.\r\n");
    ls1x_drv_init();            		/* Initialize device drivers */
    install_3th_libraries();      		/* Install 3th libraies */
    ls1x_uart_init(devUART4,NULL); //��ʼ������:������Ļ
    ls1x_uart_open(devUART4,NULL);
    ls1x_uart_init(devUART1,NULL); //��ʼ�����ڣ�esp32��4g
    ls1x_uart_open(devUART1,NULL);
    gpio_enable(WATER_IN, DIR_OUT); //��ʼ��GPIO:2���������
    gpio_enable(WATER_OUT, DIR_OUT);
    gpio_write(WATER_IN,0);
    gpio_write(WATER_OUT,0);
    gpio_enable(54, DIR_OUT);
    gpio_write(54,1);


    
    xTaskCreate(uart_screen_task, // �����������
                "uartscreentask",
                2048,
				NULL,
                31,
				NULL);
    xTaskCreate(uart_stm32_task, // ����esp32���ڲ����͵���Ļ��4gģ��
                "uartstm32task",
                2048,
				NULL,
                31,
				&uart_stm32_task_handle);
    xTaskCreate(water_task, // ˮ��
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

