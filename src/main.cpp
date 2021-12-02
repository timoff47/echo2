#include <Arduino.h>
#include <SPI.h>
#include <OneWire.h>
/*

//opcodes
#define PU          0x01
#define STOP        0x02
#define RESET       0x03
#define CLR_INT     0x04
#define RD_STATUS   0x05
#define RD_PLAY_PTR 0x06
#define PD          0x07
#define RD_REC_PTR  0x08
#define DEVID       0x09
#define PLAY        0x40
#define REC         0x41
#define ERASE       0x42
#define G_ERASE     0x43 G_ERASE Erase all messages except Sound Effects
#define RD_APC      0x44
#define WR_APC1     0x45
#define WR_APC2     0x65
#define WR_NVCFG    0x46
#define LD_NVCFG    0x47
#define FWD         0x48
#define CHK_MEM     0x49
#define EXTCLK      0x4A
#define SET_PLAY    0x49
#define SET_REC     0x81
#define SET_ERASE   0x82

*/

//opcodes для SPI ISD17XX
#define PU          0x01
#define CLR_INT     0x04
#define PLAY        0x40
#define REC         0x41
#define REC_LED     0x51
#define G_ERASE     0x43
#define ERASE       0x42
#define STOP        0x02
#define FWD         0x48
const int Slaveselect=10;

const int sqlPin = 9; // SQL вход шумоподавителя
int sqlState = 0; // флаг состояние входа SQL почему не bool?
bool playMessage = 0; // флаг сообщения, проигровалось или нет.
const int PTT = 8; // выход пина PTT

// Таймеры
uint32_t SQLStart; //начало времени открытия шумоподавителя
uint32_t SQLStop;  //конец  время закрытия шумоподавителя
uint32_t SQLTimer = 0; //расчётное время - длительность.
uint32_t SQLMaxTime; // максимально открытое время PTT, чтобы не открыт был бесконечно в случае сбоя. Пока не применил 
bool OnTimer; // флаг сработки таймера, нужен для метки что начальное время запомнено и больше не записывать время в переменную SQLStart. 

// Температурный датчик
OneWire ds(7); // Вход датчика
int temperature = 0; // Переменная для записи температуры
int maxtemp = 50; //предел температуры срабатывания аварии
long lastUpdateTime = 0; // Переменная для хранения времени последнего считывания с датчика
const int TEMP_UPDATE_TIME = 30000; // Определяем периодичность проверок датчика
bool crash = 0; // флаг ошибки 0 есть 1 нет, чтобы отключить передачу если температура > maxtemp

// QTH отбивки 
bool MorzeFlag = 0;  // флаг указывающий для функции ptton(), чтобы не закрывать передачу если передаём QTH
long lastUpdateTimeMorze = 0; // Переменная для хранения времени последнего запуска функции.
const long MORZE_UPDATE_TIME = 300000; // Определяем периодичность запуска функции.
char QTHCode[6] = {'M','O','0','5','Q','E'}; //QTH локатор
int DashDuration = 350; // Длительность тире
int DashTone = 1000; // Частота тона тире
int DotDuration = 100;  // Длительность точки
int DotTone = 1000;  // Частота тона точки
int separat_pause = 50; // Разделение между точками и тире
int pause = 500; // Разделение символов
const int PINtone = 6; // Номер пина

//счётчик сообщений использовался на время теста по серийному порту.
int countmessage = 0;

void setup() {
  Serial.begin(9600);

  SPI.begin();
  SPI.setBitOrder(LSBFIRST);
  SPI.setDataMode(SPI_MODE3);
  pinMode(Slaveselect,OUTPUT);
  
  digitalWrite(Slaveselect,LOW);
  SPI.transfer(PU); // power up
  SPI.transfer(0x00); 
  digitalWrite(Slaveselect,HIGH);
  delay(100);
 
  digitalWrite(Slaveselect,LOW);
  SPI.transfer(CLR_INT); // clear interupt 
  SPI.transfer(0x00); 
  digitalWrite(Slaveselect,HIGH);
  delay(100);
 
  pinMode(sqlPin, INPUT); //определяем вывод
  pinMode(PTT,OUTPUT);    //определяем вывод
  digitalWrite(PTT,HIGH); //закрываем PTT

  //Тон
  pinMode(6, OUTPUT); // для морзе qth
 
}

// Точка
int dotmorze() {

  tone(PINtone,DotTone,DotDuration);
  delay(DotDuration);
  noTone(PINtone); 
  delay(separat_pause);

  return 0;
}

// Тире
int dashmorze() {

  tone(PINtone,DashTone,DashDuration);
  delay(DashDuration);
  noTone(PINtone);
  delay(separat_pause);

  return 0;
}

// Пауза
int pausemorze() {

  delay(pause);

  return 0;
}

//Функция отправки QTH
int sendmorze() {

if (millis() - lastUpdateTimeMorze > MORZE_UPDATE_TIME)
{
  lastUpdateTimeMorze = millis();
  MorzeFlag = 1;
  digitalWrite(PTT,LOW); // Активируем передачу.
  delay(500); // Задержка, чтобы не съедало начало передачи инерция открытия шумодава.
  noTone(PINtone); // Убираем любой тон остаточный.
  
  Serial.print("-----------------");
  Serial.println();
  Serial.print("MORZE START ");
  Serial.print(MorzeFlag);
  Serial.println();
 for ( unsigned int i=0; i<sizeof(QTHCode)/sizeof(QTHCode[0]); i=i+1)
 {
   Serial.print("----SEND CHARS----");
   Serial.println();
   Serial.print(QTHCode[i]);
   Serial.println();


  switch (QTHCode[i])
  {
   
   case 'A':
    dotmorze();
    dashmorze();
    pausemorze();
   break;
   
   case 'B':
    dashmorze();
    dotmorze();
    dotmorze();
    dotmorze();
    pausemorze();
   break;
   
   case 'C':
    dashmorze();
    dotmorze();
    dashmorze();
    dotmorze();
    pausemorze();
   break;
  
   case 'D':
    dashmorze();
    dotmorze();
    dotmorze();
    pausemorze();
   break;

   case 'E':
    dotmorze();
    pausemorze();
   break;

   case 'F':
    dotmorze();
    dotmorze();
    dashmorze();
    dotmorze();
    pausemorze();
   break;
   
   case 'G':
    dashmorze();
    dashmorze();
    dotmorze();
    pausemorze();
   break;

   case 'H':
    dotmorze();
    dotmorze();
    dotmorze();
    dotmorze();
    pausemorze();
   break;

   case 'I':
    dotmorze();
    dotmorze();
    pausemorze();
   break;

   case 'J':
    dotmorze();
    dashmorze();
    dashmorze();
    dashmorze();
    pausemorze();
   break;

   case 'K':
    dashmorze();
    dotmorze();
    dashmorze();
    pausemorze();
   break;

   case 'L':
    dotmorze();
    dashmorze();
    dotmorze();
    dotmorze();
    pausemorze();
   break;

   case 'M':
    dashmorze();
    dashmorze();
    pausemorze();
   break;
   
   case 'N':
    dashmorze();
    dotmorze();
    pausemorze();
   break;  
   
   case 'O': 
    dashmorze();
    dashmorze();
    dashmorze();
    pausemorze();
   break;
   
   case 'P':
    dotmorze();
    dashmorze();
    dashmorze();
    dotmorze();
    pausemorze();
   break;

   case 'Q':
    dashmorze();
    dashmorze();
    dotmorze();
    dashmorze();
    pausemorze();
   break;
   
   case 'R':
    dotmorze();
    dashmorze();
    dotmorze();
    pausemorze();
   break;
   
   case 'S':
    dotmorze();
    dotmorze();
    dotmorze();
    pausemorze();
   break;

   case 'T':
    dashmorze();
    pausemorze();
   break;

   case 'U':
    dotmorze();
    dotmorze();
    dashmorze();
    pausemorze();
   break; 

   case 'V':
    dotmorze();
    dotmorze();
    dotmorze();
    dashmorze();
    pausemorze();
   break;

   case 'W':
    dotmorze();
    dashmorze();
    dashmorze();
    pausemorze();
   break;

   case 'X':
    dashmorze();
    dotmorze();
    dotmorze();
    dashmorze();
    pausemorze(); 
   break; 
   
   case 'Y':
    dashmorze();
    dotmorze();
    dashmorze();
    dashmorze();
    pausemorze();
   break;

   case 'Z':
    dashmorze();
    dashmorze();
    dotmorze();
    dotmorze();
    pausemorze();
   break;  

   case '1': 
    dotmorze();
    dashmorze();
    dashmorze();
    dashmorze();
    dashmorze();
    pausemorze();
   break;
   
   case '2': 
    dotmorze();
    dotmorze();
    dashmorze();
    dashmorze();
    dashmorze();
    pausemorze();
   break;

   case '3': 
    dotmorze();
    dotmorze();
    dotmorze();
    dashmorze();
    dashmorze();
    pausemorze();
   break;
   
   case '4': 
    dotmorze();
    dotmorze();
    dotmorze();
    dotmorze();
    dashmorze();
    pausemorze();
   break;
   
   case '5':
    dotmorze();
    dotmorze();
    dotmorze();
    dotmorze();
    dotmorze();
    pausemorze();
   break;
   
   case '6':
    dashmorze();
    dotmorze();
    dotmorze();
    dotmorze();
    dotmorze();
    pausemorze();
   break;
   
   case '7':
    dashmorze();
    dashmorze();
    dotmorze();
    dotmorze();
    dotmorze();
    pausemorze();
   break;
   
   case '8':
    dashmorze();
    dashmorze();
    dashmorze();
    dotmorze();
    dotmorze();
    pausemorze();
   break;

   case '9':
    dashmorze();
    dashmorze();
    dashmorze();
    dashmorze();
    dotmorze();
    pausemorze();
   break;

   case '0': 
    dashmorze();
    dashmorze();
    dashmorze();
    dashmorze();
    dashmorze();
    pausemorze();
   break;

   default:
   pausemorze();
    break;
  }


 }
  //noTone(PINtone);
  //delay(500);
  digitalWrite(PTT,HIGH); // закрываем передачу.
 
 
  MorzeFlag = 0;

  Serial.print("-----------------");
  Serial.println();
  Serial.print("MORZE STOP ");
  Serial.print(MorzeFlag);
  Serial.println();
}

return 0;
}

//Функция считывания датчика температуры и установка флага ошибки
int stemp(){

byte addr[8]; //хранит адрес устройства
byte data[2]; // Место для значения температуры
ds.search(addr); //ищем адрес устройства и помещаем его в массив.
ds.reset(); // Начинаем взаимодействие со сброса всех предыдущих команд и параметров
ds.select(addr);  //выбираем адрес
ds.write(0x44, 0); // Даем датчику DS18b20 команду измерить температуру. Само значение температуры мы еще не получаем - датчик его положит во внутреннюю память
  
if (millis() - lastUpdateTime > TEMP_UPDATE_TIME)
  {
    lastUpdateTime = millis();

    ds.reset(); // Теперь готовимся получить значение измеренной температуры
    ds.select(addr);
    ds.write(0xBE, 0); // Просим передать нам значение регистров со значением температуры

    // Получаем и считываем ответ
    data[0] = ds.read(); // Читаем младший байт значения температуры
    data[1] = ds.read(); // А теперь старший

  // Формируем итоговое значение: 
  //    - сперва "склеиваем" значение, 
  //    - затем умножаем его на коэффициент, соответсвующий разрешающей способности (для 12 бит по умолчанию - это 0,0625)
    temperature =  ((data[1] << 8) | data[0]) * 0.0625;
  
    Serial.print("-----------------");
    Serial.println();
    Serial.print("TEMP=");
    Serial.print(temperature);
    Serial.println();
    Serial.print("StatusMessage=");
    Serial.print(playMessage);
    Serial.println();
    Serial.print("StatusSQL=");
    Serial.print(sqlState);
    Serial.println();
    Serial.print("Error=");
    Serial.print(crash);
    Serial.println();
    Serial.print("TotalMessage=");
    Serial.print(countmessage);
    Serial.println();
    Serial.print("-----------------");
 
  }

if (temperature > maxtemp) {
  
  // ставим флаг аварии - температура превышена.
  crash = 1;
  Serial.print("Error overload Temp=");
  Serial.print(crash);
  Serial.println();
} 
else {
  crash = 0;
}

return 0;

}

// функция проверки и работы PTT
int ptton ()
{
 // Вычислить значение сколько времени был открыт SQL.  SQLTimer = SQLStop - SQLstart Вот на это время должен быть открыт PTT при передаче сообщения.
 SQLTimer = SQLStop - SQLStart;
 //Вычисляем сколько держать открытой PTT
 //Если обноружено открытие передачи PTT LOW, начинаем смотреть когда его закрывать. Если опять же PTT не открыла функция передачи QTH - флаг MorzeFlag.
 if (((digitalRead(PTT)==LOW) && (millis()-SQLStop >= SQLTimer) && (MorzeFlag == 0)))
  {
   
   digitalWrite(PTT,HIGH);
  }
return 0;
}


void loop() {

ptton();
stemp();
sendmorze();

sqlState = digitalRead(sqlPin);

/* Если открылся шумодав, то записываем новое сообщение и ставим метку о том, что не проигровылось сообщение.
   так же засекаем время. sqlState - значение при открытии шумодава LOW - открыт */

if ((sqlState == LOW) && (crash == 0))
{
   playMessage = 0;  // ставим флаг нового сообщения
   
  if ((playMessage == 0) && (OnTimer == 0)) //сообщение не проигрывалось, таймер не активировался, температура в норме
  {
      SQLStart = millis(); //Запоминаем время открытия шумодава.
      OnTimer = 1; // ставим флаг, что запомнили время и больше не перезапоминаем.
    
      Serial.print(">>>>>>>>>>>RECIVE RECORD:=");
      Serial.print(SQLStart);
      Serial.println();
      
     // Шлём модулю ISD комманду всё стереть
      digitalWrite(Slaveselect,LOW);
      delay(100);
      SPI.transfer(ERASE); // erase
      SPI.transfer(0x00);
      digitalWrite(Slaveselect,HIGH);
      delay(100);

     // Включить запись
      digitalWrite(Slaveselect,LOW);
      delay(100);
      SPI.transfer(REC_LED); // rec
      SPI.transfer(0x00);
      digitalWrite(Slaveselect,HIGH);
 

      Serial.print("StartTime=");
      Serial.print(SQLStart);
      Serial.println();
  }
}


// Если шумодав закрыт и метка о сообщении стоит, что не проигрывалось, то открываем PTT и играем сообщение.
if ((sqlState == HIGH) && (playMessage == 0) && (crash == 0))
{
  SQLStop = millis(); // Получить значение времени в глобальную переменную SQLStop это значит, что шумодав закрылся.
  OnTimer = 0; // сбрасываем флаг старта запоминания времени открытия шумодава.
  digitalWrite(PTT,LOW); // Активируем передачу.

  // Остановить. Вот после этой команды, выбор следующего не работает. FW
  digitalWrite(Slaveselect,LOW);
  delay(100);
  SPI.transfer(STOP); // rec
  SPI.transfer(0x00);
  digitalWrite(Slaveselect,HIGH);
  delay(100);

  // проиграть сообщение. 
  digitalWrite(Slaveselect,LOW);
  SPI.transfer(PLAY); // play
  SPI.transfer(0x00); // data byte
  digitalWrite(Slaveselect,HIGH);
 
  playMessage = 1; // проиграли сообщение и записали значение о проигрыше сообщения
  
  Serial.print("<<<<<<<<<<<TRANSMIT:");
  Serial.println();
  Serial.print("playmessage=");
  Serial.print(playMessage);
  Serial.println();
  Serial.print("SQLTimer=");
  Serial.print(SQLStop-SQLStart);
  Serial.println();
 
  countmessage++; // считаем сообщения, для вывода статистики в порт.
    }
}
