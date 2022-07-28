#include <DHT.h>//온습도센서
#include <DHT_U.h>//온습도센서
#include <LiquidCrystal_I2C.h>/LCD모니터사용
#include <SoftwareSerial.h>//블루투스모듈사용

#include <avr/wdt.h>//워치독 타이머 일정 시간 동안 리셋되지 않으면 아두이노를 리셋

SoftwareSerial BTSerial(2,3);//Tx: digital 2 Rx: digital 3
LiquidCrystal_I2C lcd(0x27, 20, 4);
extern volatile unsigned long timer0_millis;

#define PUMPTIMER 120000 //110초 이후 센서가 작동하지 않아도 펌프를 자동으로 종료
#define PUMPTIMING 1800000 //30분마다 펌프 작동=> 60*30*1000
#define LCDTIMING 30000//30초마다 LCD업데이트 -LCD오류 방지
#define BTTIMING 10000//10초마다 블루투스 통신
 
void lcd_control(double currentHour, int watering_count,bool led_check,bool bt_available){

  lcd.setCursor(0, 0);   
  lcd.print("BT connect : ");//14
  if(bt_available){
    lcd.setCursor(14,0);
    lcd.print("O");
  }
  else{
    lcd.setCursor(14,0);
    lcd.print("X");
  }
  lcd.setCursor(0, 1);
  lcd.print("Time 0-24 : ");//13
  lcd.setCursor(13,1);
  lcd.print(currentHour);
  lcd.setCursor(0, 2);
  lcd.print("Watered: ");//9
  lcd.setCursor(9, 2);
  lcd.print(watering_count);
  lcd.setCursor(0, 3);
  lcd.print("Led state: ");//11
  lcd.setCursor(11, 3);
  if(led_check==false){
    lcd.print("light OUT");
  }
  else{
    lcd.print("light ON");
  }
}

//물주기, 수위조절 current_time을 이용해 센서가 작동하지 않을 때를 대비함-일정시간후 펌프종료,반환값은 센서 작동여부 true=>작동
bool watering(unsigned int long start_time){
  unsigned int long limit_time=PUMPTIMER;
  unsigned int long current_time=0;
  //lcd업데이트
  lcd.clear();
  lcd.setCursor(0,1);
  lcd.print("Watering!!!");

  while(1){
    //워치독 타이머를 리셋
    wdt_reset();
    //센서가 작동하면 펌프작동을 중단하고 true를 반환
    digitalWrite(9, HIGH);//펌프작동
    if(analogRead(A1)>=400){//수위조절센서
      digitalWrite(9,LOW);
      lcd.clear();
      return true;
    }
    //센서가 작동하지 않았지만 일정 시간이 지나면 펌프 작동을 중단하고 false를 반환
    current_time=millis();//현재시간
    if((current_time-start_time)>=limit_time){//펌프작동후 경과된 시간을 limit_time과 비교
      digitalWrite(9,LOW);
      lcd.clear();
      lcd.setCursor(0,1);
      lcd.print("Sensor fail");
      wdt_disable();//delay함수를 사용하기 위해 워치독을 비활성화
      delay(10000);
      lcd.clear();
      wdt_enable(WDTO_8S);//워치독 다시 사용
      return false;
    }
  }
}

//기본 셋업
void setup()
{
  lcd.begin(); //LCD 사용 시작 20x4 lcd임
  delay(2000);//여기서 delay를 사용하니 초기의 lcd오류가 덜남
  BTSerial.begin(9600);
  pinMode(8, OUTPUT);//led용 릴레이
  pinMode(9, OUTPUT);//펌프용 릴레이
  DHT dht(A5, DHT11);
  dht.begin();//온습도센서
  wdt_enable(WDTO_8S);
}

bool triggerByTime(unsigned int long currentTime, int time_check, unsigned int long timing){
  int current_state=currentTime/timing;
  if(current_state!=time_check){
    return true;
  }
  else{
    return false;
  }
  /*time_check는 기계가 작동을 시작하면 0에서 시작한다. current_state는 항시 시간을 timing에서 지정한 단위로 업데이트 한다. 
    만약 current_state가 새로운 값으로 업데이트되면 time_check와 값이 달라지게되고 if문을 통해 이를 알아낼 수 있다.
    변화를 감지한 이후에 true를 반환하여 의도한 어떤 함수가 작동할 수 있게 한다. 또한 time_check는 성공적으로 timing에서 지정한 시간 단위 시간 변화를
    감지후 기존의 current_state와 동일한 값으로 바꾼다.(함수외부에서)*/
}

//메인함수
void loop()
{
  int watering_count=0;//24시간 주기안에서 물을 준 횟수를 카운트
  int time_check_w=0;//30분마다 물을 주는것을 보장하기위한 변수
  int time_check_l=0;//30초마다 lcd를 업데이트 하는것을 보장하기 위한 변수=>lcd오류 방지
  int time_check_b=0;//10초마다 블루투스통신
  bool led_check=true;//led의 점등,소등여부
  bool bt_available=false;//블루투스 동작여부
  float temp=0;//온도
  float hum=0;//습도

  unsigned int long pumptiming=PUMPTIMING;//아두이노에서 int의 범위는 -32768~32767
  unsigned int long lcdtiming=LCDTIMING;
  unsigned int long BTtiming=BTTIMING;
  
  unsigned int long currentTime=millis();
  //초기 작동
  digitalWrite(8,HIGH);//led작동
  if(watering(currentTime)){
    watering_count++;
  }

  //메인 루프
  while(1){
    wdt_reset();
    currentTime=millis();//작동후 경과한 시간을 밀리초단위로 반환
    temp=dht.readTemperature();
    hum=dht.readHumidity();
    //밀리초 단위로 반환된 시간을 시간단위로 소수점으로 나타내며 lcd에 표시하기 위함
    //millis()의 반환값은 unsigned int long이므로 double로 바꾸어 연산하여야한다.
    double currentHour=floor(double(currentTime)/3600000.0*100)/100;

    if(BTserial.available()){
      bt_available=true;
    }
    else{
      bt_available=false;
    }
    //lcd업데이트를 위한 함수 진입
    //lcd표시정보 : 현재시간, 물을 준 횟수, led의 점등여부
    lcd_control(currentHour,watering_count,led_check,bt_available);

    //lcd를 일정시간마다 리셋하기위해 시간을 체크하는 변수이다.(lcd에서 발생하는 오류를 방지하기 위함이다.)
    if(triggerByTime(currentTime,time_check_l,lcdtiming)){
      time_check_l=currentTime/lcdtiming;
      lcd.begin();
    }

    //물주기를 일정시간마다 함을 확실히 하기위함이다. 30분마다 한번은 무조건 작동하여야 한다.
    if(triggerByTime(currentTime,time_check_w,pumptiming)){
      time_check_w=currentTime/pumptiming;
      if(watering(currentTime)){
        watering_count++;
      }
    }

    //10초를 주기로하여 블루투스를 통해 스마트폰으로 정보를 전송한다.-확정된 기능이아님. 블루투스통신 확인 목적
    if(triggerByTime(currentTime,time_check_b,BTtiming)){
      time_check_b=currentTime/BTtiming;
      if(BTSerial.available()){
        BTSerial.print("watering : ");
        BTSerial.println(watering_count);
        BTSerial.print("WateLevelSensor : ");
        BTSerial.println(analogRead(A1));
      }
    }

    if(currentTime>=57600000){//16시간이후 led 소등
      digitalWrite(8,LOW);
      led_check=false;//정상적으로 led 소등
   }

    if(currentTime>=86400000){//24시간이후 타이머 초기화
      break;
    }
  }//대while문
  
}
