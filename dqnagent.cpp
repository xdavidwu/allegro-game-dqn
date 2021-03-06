#include <cmath>
#include <vector>
#include <random>
#include <thread>
#include <experimental/algorithm>

#include <tensorflow/cc/client/client_session.h>
#include <tensorflow/cc/ops/standard_ops.h>
#include <tensorflow/core/framework/tensor.h>
#include <tensorflow/cc/framework/gradients.h>

using namespace tensorflow;
using namespace tensorflow::ops;

#define MEM_SIZE	1<<10
#define MEM_START_TRAIN	1<<8
#define BATCH	512
#define QT_UPDATE_INT	64
#define PRINT_INT	1024
#define DOUBLE_DQN	0
#define GAMMA	0.9f
#define LEARNING_RATE	0.0001
typedef struct{
	unsigned char s[60*90*3];
	int a;
	int r;
	unsigned char sp[60*90*3];
	int isterm;
}mem_t;

static std::vector<mem_t> replaymem(MEM_SIZE);
static int replaymem_idx=0,replaymem_full=0;

extern float epsilon;
static int train_counter=0;
static int print_counter=0;
static int block_train=0;

static Scope scope=Scope::NewRootScope();
static Placeholder x(scope,DT_FLOAT);
static Placeholder qt_x(scope,DT_FLOAT);
static Placeholder yp(scope,DT_FLOAT);

static Variable cb(scope,{16},DT_FLOAT);
static Variable cw(scope,{6,6,3,8},DT_FLOAT);
static Variable cb2(scope,{8},DT_FLOAT);
static Variable cw2(scope,{2,2,8,4},DT_FLOAT);
static Variable b(scope,{128},DT_FLOAT);
static Variable w(scope,{19*29*4,128},DT_FLOAT);
static Variable b2(scope,{3},DT_FLOAT);
static Variable w2(scope,{128,3},DT_FLOAT);

static Variable cb_ms(scope,{16},DT_FLOAT);
static Variable cw_ms(scope,{6,6,3,8},DT_FLOAT);
static Variable cb2_ms(scope,{8},DT_FLOAT);
static Variable cw2_ms(scope,{2,2,8,4},DT_FLOAT);
static Variable b_ms(scope,{128},DT_FLOAT);
static Variable w_ms(scope,{19*29*4,128},DT_FLOAT);
static Variable b2_ms(scope,{3},DT_FLOAT);
static Variable w2_ms(scope,{128,3},DT_FLOAT);
static Variable cb_mom(scope,{16},DT_FLOAT);
static Variable cw_mom(scope,{6,6,3,8},DT_FLOAT);
static Variable cb2_mom(scope,{8},DT_FLOAT);
static Variable cw2_mom(scope,{2,2,8,4},DT_FLOAT);
static Variable b_mom(scope,{128},DT_FLOAT);
static Variable w_mom(scope,{19*29*4,128},DT_FLOAT);
static Variable b2_mom(scope,{3},DT_FLOAT);
static Variable w2_mom(scope,{128,3},DT_FLOAT);

static Variable qt_cb(scope,{16},DT_FLOAT);
static Variable qt_cw(scope,{6,6,3,8},DT_FLOAT);
static Variable qt_cb2(scope,{8},DT_FLOAT);
static Variable qt_cw2(scope,{2,2,8,4},DT_FLOAT);
static Variable qt_b(scope,{128},DT_FLOAT);
static Variable qt_w(scope,{19*29*4,128},DT_FLOAT);
static Variable qt_b2(scope,{3},DT_FLOAT);
static Variable qt_w2(scope,{128,3},DT_FLOAT);

static Reshape xp(scope,x,{-1,60,90,3});
static Relu co(scope,Add(scope,Conv2D(scope,xp,cw,{1,3,3,1},"VALID"),cb));
static Relu co2(scope,Add(scope,Conv2D(scope,co,cw2,{1,1,1,1},"VALID"),cb2));
static Reshape xf(scope,co2,{-1,4*19*29});
static Relu o(scope,Add(scope,MatMul(scope,xf,w),b));
static Add y(scope,MatMul(scope,o,w2),b2);
static Max ymax(scope,y,1);
static ArgMax argmax(scope,y,1);
static Mean loss(scope,Mean(scope,Square(scope,Subtract(scope,yp,y)),1),0);

static Reshape qt_xp(scope,qt_x,{-1,60,90,1});
static Relu qt_co(scope,Add(scope,Conv2D(scope,qt_xp,qt_cw,{1,3,3,1},"VALID"),qt_cb));
static Relu qt_co2(scope,Add(scope,Conv2D(scope,qt_co,qt_cw2,{1,1,1,1},"VALID"),qt_cb2));
static Reshape qt_xf(scope,qt_co2,{-1,4*19*29});
static Relu qt_o(scope,Add(scope,MatMul(scope,qt_xf,qt_w),qt_b));
static Add qt_y(scope,MatMul(scope,qt_o,qt_w2),qt_b2);
static Max qt_ymax(scope,qt_y,1);
static std::vector<Output> qt_update({
		Assign(scope,qt_cb,cb),
		Assign(scope,qt_cw,cw),
		Assign(scope,qt_cb2,cb2),
		Assign(scope,qt_cw2,cw2),
		Assign(scope,qt_b,b),
		Assign(scope,qt_w,w),
		Assign(scope,qt_b2,b2),
		Assign(scope,qt_w2,w2)
		});

// TODO stddev for truncated normal
static std::vector<Output> q_init({
/*		Assign(scope,cb,Fill(scope,{16},float(0.0))),
		Assign(scope,cw,Multiply(scope,TruncatedNormal(scope,{6,6,3,8},DT_FLOAT),Fill(scope,{6,6,3,8},float(sqrt(2.0/(6*6*3+8)))))),
		Assign(scope,cb2,Fill(scope,{8},float(0.0))),
		Assign(scope,cw2,Multiply(scope,TruncatedNormal(scope,{2,2,8,4},DT_FLOAT),Fill(scope,{2,2,8,4},float(sqrt(2.0/(2*2*8+4)))))),
		Assign(scope,b,Fill(scope,{128},float(0.0))),
		Assign(scope,w,Multiply(scope,TruncatedNormal(scope,{19*29*4,128},DT_FLOAT),Fill(scope,{19*29*4,128},float(sqrt(2.0/(19*29*4+128)))))),
		Assign(scope,b2,Fill(scope,{3},float(0.0))),*/
		Assign(scope,w2,Multiply(scope,TruncatedNormal(scope,{128,3},DT_FLOAT),Fill(scope,{128,3},float(sqrt(2.0/(128+3))))))
		});
static std::vector<Output> rms_init({
		Assign(scope,cb_ms,ZerosLike(scope,cb)),
		Assign(scope,cw_ms,ZerosLike(scope,cw)),
		Assign(scope,cb2_ms,ZerosLike(scope,cb2)),
		Assign(scope,cw2_ms,ZerosLike(scope,cw2)),
		Assign(scope,b_ms,ZerosLike(scope,b)),
		Assign(scope,w_ms,ZerosLike(scope,w)),
		Assign(scope,b2_ms,ZerosLike(scope,b2)),
		Assign(scope,w2_ms,ZerosLike(scope,w2)),
		Assign(scope,cb_mom,ZerosLike(scope,cb)),
		Assign(scope,cw_mom,ZerosLike(scope,cw)),
		Assign(scope,cb2_mom,ZerosLike(scope,cb2)),
		Assign(scope,cw2_mom,ZerosLike(scope,cw2)),
		Assign(scope,b_mom,ZerosLike(scope,b)),
		Assign(scope,w_mom,ZerosLike(scope,w)),
		Assign(scope,b2_mom,ZerosLike(scope,b2)),
		Assign(scope,w2_mom,ZerosLike(scope,w2))
		});

static std::vector<Output> step;

static ClientSession* cs;

static std::thread* prefetch_thread=nullptr;
static std::vector<mem_t> bat(BATCH);
static std::vector<mem_t>::iterator it;

static inline float dbl_rand(){
	return (float)rand()/RAND_MAX;
}

void dqnagent_pushmem(unsigned char*s,int a,int r,unsigned char *sp,int isterm){
	if(block_train)return;
	mem_t nmem={.a=a,.r=r,.isterm=isterm};
	memcpy(nmem.s,s,sizeof(unsigned char)*60*90*3);
	memcpy(nmem.sp,sp,sizeof(unsigned char)*60*90*3);
	replaymem[replaymem_idx++]=nmem;
	if(replaymem_idx==MEM_SIZE){
		replaymem_full=1;
		replaymem_idx=0;
	}
}

void dqnagent_init(){
	SessionOptions opt;
	//opt.config.mutable_gpu_options()->set_per_process_gpu_memory_fraction(0.5);
	cs=new ClientSession(scope,opt);
	TF_CHECK_OK(cs->Run({},q_init,nullptr));
	puts("q_init passed");
	TF_CHECK_OK(cs->Run({},rms_init,nullptr));
	TF_CHECK_OK(cs->Run({},{qt_update},nullptr));
	std::vector<Output> grad_outputs;
	TF_CHECK_OK(AddSymbolicGradients(scope,{loss},{cb,cw,cb2,cw2,b,w,b2,w2},&grad_outputs));
	Input::Initializer lr(float(LEARNING_RATE)),rho(float(0.9)),ep(float(1.0e-10)),mom(float(0.0));	
	step.push_back(ApplyRMSProp(scope,cb,cb_ms,cb_mom,lr,rho,mom,ep,{grad_outputs[0]}));
	step.push_back(ApplyRMSProp(scope,cw,cw_ms,cw_mom,lr,rho,mom,ep,{grad_outputs[1]}));
	step.push_back(ApplyRMSProp(scope,cb2,cb2_ms,cb2_mom,lr,rho,mom,ep,{grad_outputs[2]}));
	step.push_back(ApplyRMSProp(scope,cw2,cw2_ms,cw2_mom,lr,rho,mom,ep,{grad_outputs[3]}));
	step.push_back(ApplyRMSProp(scope,b,b_ms,b_mom,lr,rho,mom,ep,{grad_outputs[4]}));
	step.push_back(ApplyRMSProp(scope,w,w_ms,w_mom,lr,rho,mom,ep,{grad_outputs[5]}));
	step.push_back(ApplyRMSProp(scope,b2,b2_ms,b2_mom,lr,rho,mom,ep,{grad_outputs[6]}));
	step.push_back(ApplyRMSProp(scope,w2,w2_ms,w2_mom,lr,rho,mom,ep,{grad_outputs[7]}));
}

int dqnagent_getact(unsigned char *s){
	if(dbl_rand()<epsilon){
		return rand()%3;
	}
	Tensor x_d(DT_FLOAT, TensorShape{1,60,90,3});
	std::vector<Tensor> outputs;
	for(int a=0;a<60*90*3;a++) x_d.flat<float>()(a)=(float)s[a];
	TF_CHECK_OK(cs->Run({{x,x_d}},{argmax,y},&outputs));
	return outputs[0].scalar<int>()();
}

void sample_thr(){
	if(replaymem_full) it=replaymem.end();
	else it=replaymem.begin()+replaymem_idx-1;
	std::experimental::sample(replaymem.begin(),it,bat.begin(),BATCH,std::mt19937_64{std::random_device{}()});
}

void dqnagent_train(){
	if(block_train||((!replaymem_full)&&replaymem_idx<MEM_START_TRAIN)) return;
	if(prefetch_thread==nullptr){
		if(replaymem_full) it=replaymem.end();
		else it=replaymem.begin()+replaymem_idx-1;
		std::experimental::sample(replaymem.begin(),it,bat.begin(),BATCH,std::mt19937_64{std::random_device{}()});
	}
	else{
		prefetch_thread->join();
		delete prefetch_thread;
	}
	std::vector<Tensor> outputs;
	Tensor s(DT_FLOAT, TensorShape{BATCH,60,90,3});
	Tensor sp(DT_FLOAT, TensorShape{BATCH,60,90,3});
	for(int i=0;i<BATCH;i++){
		for(int j=0;j<60*90*3;j++){
			s.flat<float>()(i*60*90*3+j)=(float)bat[i].s[j];
			sp.flat<float>()(i*60*90*3+j)=(float)bat[i].sp[j];
		}
	}
	Tensor q_,ya,ynxt,ym;
	if(DOUBLE_DQN){
		TF_CHECK_OK(cs->Run({{x,s}},{y},&outputs));
		q_=outputs[0];
		TF_CHECK_OK(cs->Run({{x,sp},{qt_x,sp}},{argmax,qt_y},&outputs));
		ya=outputs[0];
		ynxt=outputs[1];
	}
	else{
		TF_CHECK_OK(cs->Run({{qt_x,sp},{x,s}},{y,qt_ymax},&outputs));
		q_=outputs[0];
		ym=outputs[1];
	}
	float r;
	for(int i=0;i<BATCH;i++){
		r=bat[i].r;
		if(bat[i].isterm){
			q_.matrix<float>()(i,bat[i].a)=r;
		}
		else{
			if(DOUBLE_DQN){
				q_.matrix<float>()(i,bat[i].a)=r+GAMMA*ynxt.matrix<float>()(i,
						ya.matrix<int>()(i));
			}
			else{
				q_.matrix<float>()(i,bat[i].a)=r+GAMMA*ym.flat<float>()(i);
			}
		}
	}
	if(print_counter!=PRINT_INT-1){
		prefetch_thread=new std::thread(sample_thr);
	}
	TF_CHECK_OK(cs->Run({{x,s},{yp,q_}},{step},nullptr));
	if(++train_counter==QT_UPDATE_INT){
		train_counter=0;
		TF_CHECK_OK(cs->Run({},{qt_update},nullptr));
	}
	if(++print_counter==PRINT_INT){
		print_counter=0;
		TF_CHECK_OK(cs->Run({{x,s},{yp,q_}},{y,loss},&outputs));
		//for(int i=0;i<16;i++)std::cout<<bat[BATCH-1].s[i]<<" ";
		//std::cout<<"\n"<<bat[BATCH-1].a<<" "<<bat[BATCH-1].r<<"\n";
		//for(int i=0;i<16;i++)std::cout<<bat[BATCH-1].sp[i]<<" ";
		//std::cout<<bat[BATCH-1].isterm<<std::endl;
		std::cout<<outputs[1].scalar<float>()<<" "<<q_.matrix<float>()(BATCH-1,bat[BATCH-1].a)<<" "<<outputs[0].matrix<float>()(BATCH-1,bat[BATCH-1].a)<<" "<<r<<std::endl;
		prefetch_thread=new std::thread(sample_thr);
	}
}
/*
int main(){
	gen_log2();
	dqnagent_init();
	agent_t dqnagent={.getact=dqnagent_getact,.pushmem=dqnagent_pushmem,.train=dqnagent_train};
	srand(time(NULL));
	epsilon=1.0f;
	double epsilon_bak;
	while((!replaymem_full)&&replaymem_idx<MEM_START_TRAIN) episode(dqnagent);
	puts("Filled");
	FZTE(j,150){
		unsigned int tscore=0,tstep=0;
		FZTE(i,100){
			log_t elog=episode(dqnagent);
			tscore+=elog.score;
			tstep+=elog.step;
			epsilon*=0.9997f;//7f;
		}
		printf("Train %d %f %u %u\n",j,epsilon,tscore/100,tstep/100);
		epsilon_bak=epsilon;
		epsilon=0.0f;
		block_train=1;
		tscore=0;
		tstep=0;
		FZTE(i,100){
			log_t elog=episode(dqnagent);
			tscore+=elog.score;
			tstep+=elog.step;
		}
		printf("Test %d %f %u %u\n",j,epsilon,tscore/100,tstep/100);
		block_train=0;
		epsilon=epsilon_bak;
	}
	return 0;
}*/
