#include <webots/robot.h>
#include <webots/distance_sensor.h>
#include <webots/accelerometer.h>
#include <webots/motor.h>
#include <webots/supervisor.h>
#include <webots/supervisor.h>
#include <stdio.h>
#include <stdlib.h>
#include <webots/led.h>
#include <unistd.h>

#define LEDS 10
#define nr_caixas 9
#define TIME_STEP 64
#define MAX_SPEED 6.28

static const char* posicao_led[LEDS] = { "l0", "l1", "l2", "l3", "l4","l5", "l6", "l7", "l8", "l9" };
const char sensores_robo[8][4] = { "s0", "s1", "s2", "s3","s4", "s5", "s6", "s7" };
const char node_caixas[9][12] = { "c1", "c2", "c3", "c4", "c5", "c6", "c7", "c8", "c9" };

double** mapa(int linha, int coluna);
void objetoDetectado(bool detectado, WbFieldRef* caixa_def, double** caixa_posicao, WbDeviceTag* leds);
static void movimento();
static void wait(double tempo);
typedef struct sensor_obstaculos{
  bool direita;
  bool esquerda;
} sensor_obstaculos;
void velRobo(sensor_obstaculos* obstaculo, WbDeviceTag* motor_esquerdo, WbDeviceTag* motor_direito);
void iniciarRobo(const WbDeviceTag* motor_esquerdo, const WbDeviceTag* motor_direito);
static int obterMovimento();
void sensorAproximidade(WbDeviceTag* sensores_proximidade);

void attPosCaixa(double* boxInitialPosition, const double* boxCurrentPosition);
void defCaixa(WbNodeRef* caixas, WbFieldRef* caixa_def);
void defCaixaPos(WbFieldRef* caixa_def, double** caixa_posicao);

void verificarColisao(WbDeviceTag* sensores_proximidade, WbFieldRef* caixa_def, double** caixa_posicao, WbDeviceTag* leds);
void verificarObstaculos(WbDeviceTag* sensores_proximidade, sensor_obstaculos* sensor_obstaculos);

void Leds(WbDeviceTag* leds);
void ativarLed(WbDeviceTag* leds);
void defineLed(WbDeviceTag* leds);

double** mapa(int linha, int coluna){

  double** m = malloc(sizeof(double*) * linha);
  
  for(int i = 0; i < linha; i++){
      m[i] = malloc(sizeof(double) * coluna);
  }
  return m;
}

void objetoDetectado(bool detectado, WbFieldRef* caixa_def, double** caixa_posicao, WbDeviceTag* leds){
  bool objeto_movel = false;
  if (detectado){
    for(int i = 0; i < nr_caixas; i++){
        const double* posicao_box= wb_supervisor_field_get_sf_vec3f(caixa_def[i]);
        for(int j = 0; j < 3; j++){
          if(posicao_box[j] != caixa_posicao[i][j]){        
            objeto_movel = true;
            attPosCaixa(caixa_posicao[i], posicao_box);
            break;
          }  
        } 
    }
    
    if(objeto_movel)
      ativarLed(leds);  
  }     
}

static void movimento() {
    if (wb_robot_step(obterMovimento()) == -1) {
        wb_robot_cleanup();
        exit(EXIT_SUCCESS);
    }
}

static void wait(double tempo) {
    double start_time = wb_robot_get_time();
    do {
        movimento();
    } while (start_time + tempo > wb_robot_get_time());
}

void velRobo(sensor_obstaculos* obstaculo, WbDeviceTag* motor_esquerdo, WbDeviceTag* motor_direito){
  double vel_esquerda  = 0.5 * MAX_SPEED;
  double vel_direita = 0.5 * MAX_SPEED;
  
   if (obstaculo->esquerda) {
      vel_esquerda  += 0.5 * MAX_SPEED;
      vel_direita -= 0.5 * MAX_SPEED;
   }else if (obstaculo->direita) {
      vel_esquerda  -= 0.5 * MAX_SPEED;
      vel_direita += 0.5 * MAX_SPEED;
   }
   
   wb_motor_set_velocity(*motor_esquerdo, vel_esquerda);
   wb_motor_set_velocity(*motor_direito, vel_direita);

}

void iniciarRobo(const WbDeviceTag* motor_esquerdo, const WbDeviceTag* motor_direito){
  wb_motor_set_position(*motor_esquerdo, INFINITY);
  wb_motor_set_position(*motor_direito, INFINITY);
  wb_motor_set_velocity(*motor_esquerdo, 0.0);
  wb_motor_set_velocity(*motor_direito, 0.0);
}

static int obterMovimento() {
    static int time_step = -1;
    if (time_step == -1)
        time_step = (int)wb_robot_get_basic_time_step();
    return time_step;
}

void sensorAproximidade(WbDeviceTag* sensores_proximidade){  
  for (int i = 0; i < nr_caixas-1 ; i++) {
    sensores_proximidade[i] = wb_robot_get_device(sensores_robo[i]);
    wb_distance_sensor_enable(sensores_proximidade[i], TIME_STEP);
  }
}

void attPosCaixa(double* boxInitialPosition, const double* boxCurrentPosition){
  for(int i = 0; i < 3; i++){ 
     boxInitialPosition[i] = boxCurrentPosition[i];  
  }
}

void defCaixa(WbNodeRef* caixas, WbFieldRef* caixa_def){  
  for(int i = 0; i < 9; i++){
    caixas[i] = wb_supervisor_node_get_from_def(node_caixas[i]);
    caixa_def[i] = wb_supervisor_node_get_field(caixas[i], "translation");
  }
}

void defCaixaPos(WbFieldRef* caixa_def, double** caixa_posicao){
  for(int i = 0; i < 9; i++){
    const double* posicao_box = wb_supervisor_field_get_sf_vec3f(caixa_def[i]);
    for(int j = 0; j < 3; j++)
      caixa_posicao[i][j] = posicao_box[j];
  }
}

void verificarColisao(WbDeviceTag* sensores_proximidade, WbFieldRef* caixa_def, double** caixa_posicao, WbDeviceTag* leds){
  bool objeto_detectado = false;

  for(int i = 0; i < nr_caixas-1; i++){
    float sensor = wb_distance_sensor_get_value(sensores_proximidade[i]);
    if(sensor > 100){
       objeto_detectado = true;
        break; 
    }
  }
  
  objetoDetectado(objeto_detectado, caixa_def, caixa_posicao, leds);
}

void verificarObstaculos(WbDeviceTag* sensores_proximidade, sensor_obstaculos* sensor_obstaculos){
  double valor_sensores_proximidade[8];
  
  for (int i = 0; i < nr_caixas-1 ; i++){
      valor_sensores_proximidade[i] = wb_distance_sensor_get_value(sensores_proximidade[i]);
  }

  bool direita = valor_sensores_proximidade[0] > 80.0 || valor_sensores_proximidade[1] > 80.0 || valor_sensores_proximidade[2] > 80.0;
  bool esquerda = valor_sensores_proximidade[5] > 80.0 || valor_sensores_proximidade[6] > 80.0 || valor_sensores_proximidade[7] > 80.0;
     
  sensor_obstaculos->direita = direita;
  sensor_obstaculos->esquerda = esquerda;
       
}

void Leds(WbDeviceTag* leds){
  for (int i = 0; i < LEDS; i++)
    leds[i] = wb_robot_get_device(posicao_led[i]);
}


void ativarLed(WbDeviceTag* leds) {
  for (int i = 0; i < LEDS; i++)
    wb_led_set(leds[i], true);
  wait(0.5);
}

void defineLed(WbDeviceTag* leds) {
  for (int i = 0; i < LEDS; i++)
    wb_led_set(leds[i], false);
}

int main(int argc, char **argv) {
  wb_robot_init();

  double **caixa_posicao = mapa(9,3);
  sensor_obstaculos* sensor_obstaculos = malloc(sizeof(sensor_obstaculos));
  
  WbDeviceTag leds[LEDS];
  WbDeviceTag sensores_proximidade[8];
  WbDeviceTag esquerda_motor = wb_robot_get_device("left wheel motor");
  WbDeviceTag direita_motor = wb_robot_get_device("right wheel motor");
  WbNodeRef caixas[9];
  WbFieldRef caixa_def[9];

  sensorAproximidade(sensores_proximidade);
  iniciarRobo(&esquerda_motor, &direita_motor);
  defCaixa(caixas, caixa_def);
  defCaixaPos(caixa_def, caixa_posicao);
  Leds(leds);

  while (wb_robot_step(TIME_STEP) != -1) {
    velRobo(sensor_obstaculos, &esquerda_motor, &direita_motor); 
    verificarColisao(sensores_proximidade,caixa_def,caixa_posicao,leds);
    verificarObstaculos(sensores_proximidade, sensor_obstaculos);
    defineLed(leds);
  }
 
  wb_robot_cleanup();
  return 0;
}

