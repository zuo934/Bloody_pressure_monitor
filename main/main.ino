#include <Adafruit_ADS1015.h>
#include <stdio.h>
#include <string.h>
#include <Arduino.h>

/*
 * 定义了ADC
 * 走I2C
 * 16bit
 */ 
Adafruit_ADS1115 ads;

/*
 * 定义ADC的输入口
 * ADC 0口为波形
 * ADC 3口为压力
 */
const int WAVE_SIG = 0;
const int PRES_SIG = 3;

/*
 * 电机驱动板的定义
 * 电机驱动板的四个输入口
 */
int drive_input1 = 0;
int drive_input2 = 2;
int drive_input3 = 15;
int drive_input4 = 13;


/*
 * 输入压力和波形值
 * serial输出“压力，波形\n”
 */
void serial_display_pressure_and_wave(float pres, int wave){
    Serial.print("\n");
    Serial.print(pres);
    Serial.print(",");
    Serial.print(wave);
}

/*
 * 这个函数用来处理波形信号的神秘问题，并且进行缩放，固定在100-150左右
 * 问题：前几秒会变得很大有溢出
 * 把前几秒去掉，前_MAGIC_k个值去掉。
 * 缩小_MAGIC_NUM倍
 */
int pure_magic(int wave, int order){
    int _MAGIC_NUM = 50;
    int temp = 60;
    temp = wave/_MAGIC_NUM;
    if (temp>200){
      return 60;
    }
    else{
      return temp;
    }
}

/*
 * 充气函数
 * 充气保持time毫秒
 */
void inflate_for_x_ms(int time){
    openM1_closeM2();
    delay(time);
}

/*
 * 缓慢放气函数
 */
void deflate(){
    closeM1_closeM2();
}

/*
 * 快速放气函数
 * 快速放气保持time毫秒
 */
void quick_deflate_for_x_ms(int time){
    closeM1_openM2();
    delay(time);
}

/*
 * 打开OUT1和OUT2之间的电机M1，在此项目中为充气电机
 * 关闭OUT3和OUT4之间的电机M2，在此项目中为放气气阀
 */ 
void openM1_closeM2(){
    digitalWrite(drive_input1,HIGH);
    digitalWrite(drive_input2,LOW);
    digitalWrite(drive_input3,LOW);
    digitalWrite(drive_input4,LOW);
}

/*
 * 关闭OUT1和OUT2之间的电机M1，在此项目中为充气电机
 * 关闭OUT3和OUT4之间的电机M2，在此项目中为放气气阀
 */ 
void closeM1_closeM2(){
    digitalWrite(drive_input1,LOW);
    digitalWrite(drive_input2,LOW);
    digitalWrite(drive_input3,LOW);
    digitalWrite(drive_input4,LOW);
}

/*
 * 关闭OUT1和OUT2之间的电机M1，在此项目中为充气电机
 * 打开OUT3和OUT4之间的电机M2，在此项目中为放气气阀
 */ 
void closeM1_openM2(){
    digitalWrite(drive_input1,LOW);
    digitalWrite(drive_input2,LOW);
    digitalWrite(drive_input3,HIGH);
    digitalWrite(drive_input4,LOW);
}

/*
 * 把ADC的int输入转换成电压信号
 * input: int 
 * output: float
 */ 
float transfer_adc2v(int n){
    float t = (n * 0.1875)/1000;
    return t;
}

/*
 * 把压力传感器的电压信号转换成以mmHg为单位的压强信号
 * input: float
 * output: float
 */ 
float transfer_v2p(float v){
    float v1 = v/5.0;
    float v2 = v1-0.04;
    float kpa = v2/0.018;
    float mmhg = 7.5*kpa;
    return mmhg;
}


void setup(void) {
    //serial的setup
    Serial.begin(115200);
    //ADC的setup
    ads.begin();
    //电机驱动板的setup
    pinMode(drive_input1,OUTPUT);
    pinMode(drive_input2,OUTPUT);
    pinMode(drive_input3,OUTPUT);
    pinMode(drive_input4,OUTPUT);
}

void loop(void){
    // ADC的两个个输入
    int16_t adc_wave, adc_pressure;
    //从adc中读取压力信号
    adc_pressure = ads.readADC_SingleEnded(PRES_SIG);

    /*
     * 定义pres为此时的压强值
     * 显示的数据为
     * “压强值,0\n”
     */ 
    float pres = transfer_v2p(transfer_adc2v(adc_pressure));
    serial_display_pressure_and_wave(pres,0);

    /*
     * 定义测量过程中的各个参数
     *
     * ---------------以下参数可以修改---------------
     * DEFLATION_TIME 表示缓慢放气，也就是测量的时间
     * TARGET_PRESSURE 表示充气的时候的目标充气气压
     * QUICK_DEF_TIME 表示快速放气的时间 ms单位
     * 
     * ---------------以下参数不建议修改-------------
     * _SAMPLING_RATE 采样率 每秒采样次数
     * _STEP_TIME 每次充气时长 ms单位
     */ 
    const int DEFLATION_TIME = 10;
    const int TARGET_PRESSURE = 180;
    const int _SAMPLING_RATE = 100;
    const int _STEP_TIME = 100;
    const int QUICK_DEF_TIME = 20000;
    // 确定要采集几个数据点
    int DATA_ARRAY_SIZE = (DEFLATION_TIME*_SAMPLING_RATE);
    // 循环里每采样后的一次delay
    int SAMPLING_DELAY = 1000/_SAMPLING_RATE;

    // 开两个数组存储数据
    int wave_data[DATA_ARRAY_SIZE];
    float pressure_data[DATA_ARRAY_SIZE];

    /*
     * 打气功能
     * 打到TARGET_PRESSURE就停下
     * _STEP_TIME控制每一次打气时间，ms单位
     */
    if(pres<TARGET_PRESSURE){
        inflate_for_x_ms(_STEP_TIME);
    }
    /*
     *完成打气
     */
    else{
        closeM1_closeM2();
        //获取数据DATA_ARRAY_SIZE次
        for(int counter = 0; counter < DATA_ARRAY_SIZE; counter++){
            int adc_wave = ads.readADC_SingleEnded(WAVE_SIG);
            adc_pressure = ads.readADC_SingleEnded(PRES_SIG);
            pres = transfer_v2p(transfer_adc2v(adc_pressure));
            //显示 pressure和wave，中间用逗号隔开，显示两条曲线
            serial_display_pressure_and_wave(pres,pure_magic(adc_wave,counter));
            delay(SAMPLING_DELAY);
        }
        quick_deflate_for_x_ms(QUICK_DEF_TIME);
  }
}
