//
//  main.cpp
//  Allegro Game 
//  Using Allegro 5 
//
//  Created by Kerwin Tsai on 15/11/2018.
//  Copyright © 2018 Kerwin Tsai. All rights reserved.
//
#include <stdio.h>
#include <math.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>              
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_color.h>
#include <allegro5/allegro_ttf.h>    
#include <allegro5/allegro_acodec.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_native_dialog.h>
#include "object.h"

#define GAME_TERMINATE 666
#define NUMBER_BULLETS 1
#define NUMBER_BADGUY 10
#define NUMBER_COMET 10
#define SCALE_FACTOR 10
#define width 600
#define height 900
#define SCALED_WIDTH (width/SCALE_FACTOR)
#define SCALED_HEIGHT (height/SCALE_FACTOR)
#define FPS 6000

float epsilon=1.0;

extern void dqnagent_pushmem(unsigned char*s,int a,int r,unsigned char *sp,int isterm);
extern void dqnagent_init();
extern int dqnagent_getact(unsigned char *s);
extern void dqnagent_train();

int pos_x, pos_y; // The position of rectangle's left-up corner.
bool redraw = true;
bool Done = false;
bool Game_Over = false;
int ImageRad = 2;
int ImageWidth, ImageHeight;
int state = PLAYING;
bool stop;
bool right;
bool left;

//announce function for game base
void InitShip(SpaceShip &ship);
void DrawShip(SpaceShip &ship);
void MoveUp(SpaceShip &ship);
void MoveDown(SpaceShip &ship);
void MoveLeft(SpaceShip &ship);
void MoveRight(SpaceShip &ship);

//Announce function for bullet
void InitBullet(Bullet bullet[],int size);
void DrawBullet(Bullet bullet[],int size);
void FireBullet(Bullet bullet[],int size,SpaceShip &ship);
void UpdateBullet(Bullet bullet[],int size);
void CollideBullet(Bullet bullet[],int bSize,Comet comets[],int cSize, SpaceShip &ship);
//Announce function for badguy
void InitComet(Comet comets[],int size);
void DrawComet(Comet comets[],int size);
void StartComet(Comet comets[],int size);
void UpdateComet(Comet comets[],int size);
void CollideComet(Comet comet[],int cSize,SpaceShip &ship);
/*void InitEnemy(EnEmy &enemy);
void DrawEnemy(EnEmy &enemy);
void StartEnemy(EnEmy &enemy);
void MoveEnemyUp(EnEmy &enemy);
void MoveEnemyDown(EnEmy &enemy);
void MoveEnemyLeft(EnEmy &enemy);
void MoveEnemyRight(EnEmy &enemy);*/
//init object
SpaceShip ship;
Bullet bullet[NUMBER_BULLETS];
//BackGround background[NUMBER_BADGUY];
Comet comets[NUMBER_COMET];
//EnEmy enemy;


// ALLEGRO Variables
ALLEGRO_DISPLAY* display = NULL;
ALLEGRO_FONT* font = NULL;
ALLEGRO_EVENT_QUEUE* event_queue = NULL;
ALLEGRO_EVENT event;
ALLEGRO_TIMER* timerfps = NULL;
ALLEGRO_TIMER* timerflip = NULL;
ALLEGRO_FONT* font_monaco_18px = NULL;
ALLEGRO_FONT* font_monaco_25px = NULL;
ALLEGRO_BITMAP* plane = NULL;
ALLEGRO_BITMAP* get_screen = NULL;

unsigned char *getbitmap(){
    ALLEGRO_BITMAP *scr=al_get_target_bitmap();
    unsigned char *map=(unsigned char *)malloc(sizeof(unsigned char)*SCALED_WIDTH*SCALED_HEIGHT*3);
    for (int i=0;i<SCALED_WIDTH;i++) for(int j=0;j<SCALED_HEIGHT;j++){
        ALLEGRO_COLOR c=al_get_pixel(scr,i,j);
        al_unmap_rgb(c,&map[i*SCALED_HEIGHT*3+j*3],&map[i*SCALED_HEIGHT*3+j*3+1],&map[i*SCALED_HEIGHT*3+j*3+2]);
        //printf("%d %d %u %u %u",i,j,map[i*SCALED_HEIGHT*3+j*3],map[i*SCALED_HEIGHT*3+j*3+1],map[i*SCALED_HEIGHT*3+j*3+2]);
    }
    return map;
}

void writeppm(unsigned char *bitmap){
	FILE *f=fopen("test.ppm","wb");
	fprintf(f,"P6\n%d %d\n255\n",SCALED_WIDTH,SCALED_HEIGHT);
	for(int j=0;j<SCALED_HEIGHT;j++)for (int i=0;i<SCALED_WIDTH;i++) {
		fwrite(&bitmap[i*SCALED_HEIGHT*3+j*3],1,3,f);
	}
	fclose(f);
}
//MAIN FUNCTION
int main(int argc, char *argv[]) {
    dqnagent_init();
    printf("[%s:%s] \n",__FILE__,__DATE__);
    printf("Game Start\n");
    float GameTime = 0;
    int Frames = 0;
    int GameFPS = 0;

    if (!al_init()) {
        printf("Game engine Init Error!\n");
        return -1;
    }
    display = al_create_display(SCALED_WIDTH, SCALED_HEIGHT);
    
    if(!display){
        printf("Init Display Error\n");
        return -1;
    }

    al_init_primitives_addon();
    al_init_font_addon();
    al_init_ttf_addon();
    al_install_keyboard();
    al_init_image_addon();

    plane = al_load_bitmap("plane1.png");
    //get_screen = al_get_target_bitmap();

    event_queue = al_create_event_queue();
    timerfps = al_create_timer(1.0/FPS);
    timerflip = al_create_timer(1);

    ImageWidth = al_get_bitmap_width(plane);
    ImageHeight = al_get_bitmap_height(plane);

    //INIT ALL THING ON GAME
    srand(time(NULL));
    InitShip(ship);
    InitBullet(bullet,NUMBER_BULLETS);
    InitComet(comets,NUMBER_COMET);

    al_register_event_source(event_queue,al_get_display_event_source(display));
    //al_register_event_source(event_queue,al_get_keyboard_event_source());
    al_register_event_source(event_queue,al_get_timer_event_source(timerfps));
        
    al_start_timer(timerfps);
    GameTime = al_current_time();
    unsigned char *s,*sp;
    int act;
    int oscore=0,olife=ship.lives;
   int totalf=0; 
    while(!Done){       
        al_wait_for_event(event_queue,&event);

        if(event.type == ALLEGRO_EVENT_TIMER){
            redraw = true;
            FireBullet(bullet,NUMBER_BULLETS,ship);
            //UPDATE FPS
            Frames++;
	    totalf++;
            if(al_current_time() - GameTime >= 1){
                GameTime = al_current_time();
                GameFPS = Frames;
                Frames = 0;
		printf("FPS: %d\n",GameFPS);
		printf("Score: %d HP: %d\n",ship.score,ship.lives);
	    }
            if(!((totalf>>3)&0x1)){ //16f
		    sp=getbitmap();
		    if(s!=NULL){
			    dqnagent_pushmem(s,act,(ship.score-oscore)+(olife-ship.lives),sp,0);
			    dqnagent_train();
			    free(s);
		    }
		    s=sp;
		    act=dqnagent_getact(s);
		    oscore=ship.score;
		    olife=ship.lives;
		    if(act==0){
			    left=false;
			    right=false;
		    }
		    else if(act==1){
			    left=false;
			    right=true;
		    }
		    else if(act==2){
			    left=true;
			    right=false;
		    }
	    }

            //GAME STATE
            if(state == PLAYING){
                //getbitmap();
                if(keys[ESCAPE]) state = GAMEOVER;
                else if(ship.lives <= 0) state = GAMEOVER;
                else{
                    if(left)
                        MoveLeft(ship);
                    if(right)
                        MoveRight(ship);
                    if(keys[SPACE]){
                    }
                    
                    UpdateBullet(bullet, NUMBER_BULLETS);
                    StartComet(comets, NUMBER_COMET);
                    UpdateComet(comets, NUMBER_COMET);
                    CollideBullet(bullet, NUMBER_BULLETS, comets, NUMBER_COMET, ship);
                    CollideComet(comets, NUMBER_COMET, ship);
                }
            }else if(state == GAMEOVER){
		    printf("Alive for %d frames. Score: %d Eps: %f\n",totalf,ship.score,epsilon);
		    epsilon*=0.9;
		    if(s!=NULL){
			    sp=getbitmap();
			    dqnagent_pushmem(s,act,(ship.score-oscore)+(olife-ship.lives),sp,0);
			    dqnagent_train();
			    free(s);
			    free(sp);
		    }
		    totalf=0;
		    s=NULL;
		    sp=NULL;
		    InitShip(ship);
		    InitBullet(bullet,NUMBER_BULLETS);
		    InitComet(comets,NUMBER_COMET);
		    oscore=0;
		    olife=ship.lives;
		    state=PLAYING;
            }
        }else if(event.type == ALLEGRO_EVENT_DISPLAY_CLOSE){
            Done = true;    
        }
	/*else if(event.type == ALLEGRO_EVENT_KEY_DOWN){
            switch(event.keyboard.keycode){
                case ALLEGRO_KEY_ESCAPE:
                    keys[ESCAPE] = true;
                    break;
                case ALLEGRO_KEY_LEFT:
                    keys[LEFT] = true;
                    break;
                case ALLEGRO_KEY_RIGHT:
                    keys[RIGHT] = true;
                    break;

                
            }
        }else if(event.type == ALLEGRO_EVENT_KEY_UP){
            switch(event.keyboard.keycode){
                case ALLEGRO_KEY_ESCAPE:
                    keys[ESCAPE] = true;
                    break;
                case ALLEGRO_KEY_UP:
                    keys[UP] = false;
                    break;
                case ALLEGRO_KEY_DOWN:
                    keys[DOWN] = false;
                    break;
                case ALLEGRO_KEY_LEFT:
                    keys[LEFT] = false;
                    break;
                case ALLEGRO_KEY_RIGHT:
                    keys[RIGHT] = false;
                    break;
                case ALLEGRO_KEY_SPACE:
                    keys[SPACE] = false;
                    break;
            }
        }*/

        if(redraw && al_is_event_queue_empty(event_queue)){
            redraw = false;
            if(state == PLAYING){
                
                DrawShip(ship);
                DrawBullet(bullet,NUMBER_BULLETS);
                DrawComet(comets,NUMBER_COMET);
            
            }

            al_flip_display();
	    //if(al_current_time() - GameTime >= 1){
		//writeppm(getbitmap());
		//return 0;
	    //}
	    al_clear_to_color(al_map_rgb(0,0,0));
        }
    }

    al_destroy_display(display);
    return 0;
}

//SHIP FUNCTION
void InitShip(SpaceShip &ship){
    ship.x = width/2;
    ship.y = height-40;
    ship.ID = PLAYER;
    ship.lives = 12;
    ship.speed = 5.46;
    ship.boundx = ImageWidth/2-2;
    ship.boundy = ImageHeight/2 - ImageHeight/3;
    ship.score = 0;
}
void DrawShip(SpaceShip &ship){
    al_draw_scaled_bitmap(plane,0,0,ImageWidth,ImageHeight, (ship.x-ImageWidth/2)/SCALE_FACTOR, (ship.y-ImageHeight/2)/SCALE_FACTOR,ImageWidth/SCALE_FACTOR,ImageHeight/SCALE_FACTOR, 0);
}
void MoveUp(SpaceShip &ship){
    ship.y -= ship.speed;
    ship.y = (ship.y < 15) ? 15 : ship.y;
}
void MoveDown(SpaceShip &ship){
    ship.y += ship.speed;
    ship.y = (ship.y > height-40) ? height-40 : ship.y;
}
void MoveLeft(SpaceShip &ship){
    ship.x -= ship.speed;
    ship.x = (ship.x < 17) ? 0 : ship.x;
}
void MoveRight(SpaceShip &ship){
    ship.x += ship.speed;
    ship.x = (ship.x > width) ? width : ship.x;   
}



//BULLET FUNCTION
void InitBullet(Bullet bullet[],int size){
    for(int i=0;i<size;i++){
        bullet[i].ID=BULLENT;
        bullet[i].speed=10;
        bullet[i].live=false;
    }
}
void DrawBullet(Bullet bullet[],int size){
    for(int i=0;i<size;i++){
        if(bullet[i].live) 
            al_draw_filled_circle(bullet[i].x/SCALE_FACTOR,bullet[i].y/SCALE_FACTOR,3.0/SCALE_FACTOR,al_map_rgb(253,2,255));
            //al_draw_filled_circle(bullet[i].x+9,bullet[i].y,3,al_map_rgb(253,2,255));
    }
}
void FireBullet(Bullet bullet[],int size,SpaceShip &ship){
    for(unsigned int i = 0;i < size; i++){
        if(!bullet[i].live){
            bullet[i].x = ship.x;
            bullet[i].y = ship.y - ImageHeight/2; 
            bullet[i].live = true;
            break;
        }
    }
}
void UpdateBullet(Bullet bullet[],int size){
    for(int i=0;i<size;i++){
        if(bullet[i].live){
            bullet[i].y-=bullet[i].speed;
            if(bullet[i].y <10)
                bullet[i].live=false;

        }
    }
}
void CollideBullet(Bullet bullet[],int bSize,Comet comets[],int cSize, SpaceShip &ship){
    for(unsigned int i = 0; i < bSize; i++){
        if(bullet[i].live){
            for(int j = 0; j < cSize; j++){
                if(comets[j].live){
                    if(bullet[i].x < (comets[j].x + comets[j].boundx) &&
                        bullet[i].x > (comets[j].x - comets[j].boundx) &&
                        bullet[i].y < (comets[j].y + comets[j].boundy) &&
                        bullet[i].y > (comets[j].y - comets[j].boundy)){

                            bullet[i].live = false;
                            comets[j].live = false;
                            ship.score++;

                    }
                }
            }
        }
    }
}

//COMET FUNCTION
void InitComet(Comet comets[],int size){
    for(int i=0;i<size;i++){
        comets[i].ID = COMET;
        comets[i].live = false;
        comets[i].speed = 3.256;
        comets[i].boundx = 18;
        comets[i].boundy = 18;
    }
}
void DrawComet(Comet comets[],int size){
    for(int i=0;i<size;i++){
        if(comets[i].live)
            al_draw_filled_circle(comets[i].x/SCALE_FACTOR,comets[i].y/SCALE_FACTOR,18/SCALE_FACTOR,al_map_rgb(99,99,99));
    }
}
void StartComet(Comet comets[],int size){
    for(int i = 0; i < size; i++){
        if(!comets[i].live){
            if(rand()%500 == 0){
                comets[i].live = true;
                comets[i].y = 30;
                comets[i].x = 30 + rand() % (width - 60);
                break;
            }
        }
    }
}
void UpdateComet(Comet comets[],int size){
    for(int i = 0; i < size; i++){
        if(comets[i].live){
            comets[i].y += comets[i].speed;
            if(comets[i].y > height)
                comets[i].live = false;
        }
    }
}
void CollideComet(Comet comets[],int cSize,SpaceShip &ship){
    for(unsigned int i=0;i<cSize;i++){
        if(comets[i].live){
            if(comets[i].x+comets[i].boundx>ship.x-ship.boundx&&
                comets[i].x-comets[i].boundx<ship.x+ship.boundx&&
                comets[i].y+comets[i].boundy>ship.y-ship.boundy&&
                comets[i].y-comets[i].boundy<ship.y+ship.boundy){
                    ship.lives--;
                    comets[i].live=false;
                }else if(comets[i].y > height+20){
                    comets[i].live=false;
                    ship.lives--;
                    
                }
        }
    }
}
