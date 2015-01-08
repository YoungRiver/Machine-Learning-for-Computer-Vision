#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "svm.h"

#include "mex.h"
#include "svm_model_matlab.h"

#if MX_API_VER < 0x07030000
typedef int mwIndex;
#endif 

#define CMD_LEN 2048

void read_sparse_instance(const mxArray *prhs, int index, struct svm_node *x)
{
	int i, j, low, high;
	mwIndex *ir, *jc;
	double *samples;

	ir = mxGetIr(prhs);
	jc = mxGetJc(prhs);
	samples = mxGetPr(prhs);


	j = 0;
	low = jc[index], high = jc[index+1];
	for(i=low;i<high;i++)
	{
		x[j].index = ir[i] + 1;
		x[j].value = samples[i];
		j++;
 	}
	x[j].index = -1;
}

static void fake_answer(mxArray *plhs[])
{
	plhs[0] = mxCreateDoubleMatrix(0, 0, mxREAL);
	plhs[1] = mxCreateDoubleMatrix(0, 0, mxREAL);
	plhs[2] = mxCreateDoubleMatrix(0, 0, mxREAL);
}

void predict(mxArray *plhs[], const mxArray *prhs[], struct svm_model *model, const int predict_probability)
{
	int label_vector_row_num, label_vector_col_num;
	int feature_number, testing_instance_number;
	int instance_index;
	double *ptr_instance, *ptr_label, *ptr_predict_label; 
	double *ptr_prob_estimates, *ptr_dec_values, *ptr;
	struct svm_node *x;
	mxArray *pplhs[1];

	int correct = 0;
	int total = 0;
	double error = 0;
	double sumv = 0, sumy = 0, sumvv = 0, sumyy = 0, sumvy = 0;

	int svm_type=svm_get_svm_type(model);
	int nr_class=svm_get_nr_class(model);
	double *prob_estimates=NULL;


	feature_number = mxGetN(prhs[1]);
	testing_instance_number = mxGetM(prhs[1]);
	label_vector_row_num = mxGetM(prhs[0]);
	label_vector_col_num = mxGetN(prhs[0]);

	if(label_vector_row_num!=testing_instance_number)
	{
		mexPrintf("Length of label vector does not match # of instances.\n");
		fake_answer(plhs);
		return;
	}
	if(label_vector_col_num!=1)
	{
		mexPrintf("label (1st argument) should be a vector (# of column is 1).\n");
		fake_answer(plhs);
		return;
	}

	ptr_instance = mxGetPr(prhs[1]);
	ptr_label    = mxGetPr(prhs[0]);
	

	if(mxIsSparse(prhs[1]))
	{
		if(model->param.kernel_type == PRECOMPUTED)
		{

			mxArray *rhs[1], *lhs[1];
			rhs[0] = mxDuplicateArray(prhs[1]);
			if(mexCallMATLAB(1, lhs, 1, rhs, "full"))
			{
				mexPrintf("Error: cannot full testing instance matrix\n");
				fake_answer(plhs);
				return;
			}
			ptr_instance = mxGetPr(lhs[0]);
			mxDestroyArray(rhs[0]);
		}
		else
		{
			mxArray *pprhs[1];
			pprhs[0] = mxDuplicateArray(prhs[1]);
			if(mexCallMATLAB(1, pplhs, 1, pprhs, "transpose"))
			{
				mexPrintf("Error: cannot transpose testing instance matrix\n");
				fake_answer(plhs);
				return;
			}
		}
	}

	if(predict_probability)
	{
		if(svm_type==NU_SVR || svm_type==EPSILON_SVR)
			mexPrintf("Prob. model for test data: target value = predicted value + z,\nz: Laplace distribution e^(-|z|/sigma)/(2sigma),sigma=%g\n",svm_get_svr_probability(model));
		else
			prob_estimates = (double *) malloc(nr_class*sizeof(double));
	}

	plhs[0] = mxCreateDoubleMatrix(testing_instance_number, 1, mxREAL);
	if(predict_probability)
	{

		if(svm_type==C_SVC || svm_type==NU_SVC)
			plhs[2] = mxCreateDoubleMatrix(testing_instance_number, nr_class, mxREAL);
		else
			plhs[2] = mxCreateDoubleMatrix(0, 0, mxREAL);
	}
	else
	{

		if(svm_type == ONE_CLASS ||
		   svm_type == EPSILON_SVR ||
		   svm_type == NU_SVR)
			plhs[2] = mxCreateDoubleMatrix(testing_instance_number, 1, mxREAL);
		else
			plhs[2] = mxCreateDoubleMatrix(testing_instance_number, nr_class*(nr_class-1)/2, mxREAL);
	}

	ptr_predict_label = mxGetPr(plhs[0]);
	ptr_prob_estimates = mxGetPr(plhs[2]);
	ptr_dec_values = mxGetPr(plhs[2]);
	x = (struct svm_node*)malloc((feature_number+1)*sizeof(struct svm_node) );
	for(instance_index=0;instance_index<testing_instance_number;instance_index++)
	{
		int i;
		double target,v;

		target = ptr_label[instance_index];

		if(mxIsSparse(prhs[1]) && model->param.kernel_type != PRECOMPUTED)
			read_sparse_instance(pplhs[0], instance_index, x);
		else
		{
			for(i=0;i<feature_number;i++)
			{
				x[i].index = i+1;
				x[i].value = ptr_instance[testing_instance_number*i+instance_index];
			}
			x[feature_number].index = -1;
		}

		if(predict_probability) 
		{
			if(svm_type==C_SVC || svm_type==NU_SVC)
			{
				v = svm_predict_probability(model, x, prob_estimates);
				ptr_predict_label[instance_index] = v;
				for(i=0;i<nr_class;i++)
					ptr_prob_estimates[instance_index + i * testing_instance_number] = prob_estimates[i];
			} else {
				v = svm_predict(model,x);
				ptr_predict_label[instance_index] = v;
			}
		}
		else
		{
			v = svm_predict(model,x);
			ptr_predict_label[instance_index] = v;

			if(svm_type == ONE_CLASS ||
			   svm_type == EPSILON_SVR ||
			   svm_type == NU_SVR)
			{
				double res;
				svm_predict_values(model, x, &res);
				ptr_dec_values[instance_index] = res;
			}
			else
			{
				double *dec_values = (double *) malloc(sizeof(double) * nr_class*(nr_class-1)/2);
				svm_predict_values(model, x, dec_values);
				for(i=0;i<(nr_class*(nr_class-1))/2;i++)
					ptr_dec_values[instance_index + i * testing_instance_number] = dec_values[i];
				free(dec_values);
			}
		}

		if(v == target)
			++correct;
		error += (v-target)*(v-target);
		sumv += v;
		sumy += target;
		sumvv += v*v;
		sumyy += target*target;
		sumvy += v*target;
		++total;
	}
	if(svm_type==NU_SVR || svm_type==EPSILON_SVR)
	{
		mexPrintf("Mean squared error = %g (regression)\n",error/total);
		mexPrintf("Squared correlation coefficient = %g (regression)\n",
			((total*sumvy-sumv*sumy)*(total*sumvy-sumv*sumy))/
			((total*sumvv-sumv*sumv)*(total*sumyy-sumy*sumy))
			);
	}
	else




	plhs[1] = mxCreateDoubleMatrix(3, 1, mxREAL);
	ptr = mxGetPr(plhs[1]);
	ptr[0] = (double)correct/total*100;
	ptr[1] = error/total;
	ptr[2] = ((total*sumvy-sumv*sumy)*(total*sumvy-sumv*sumy))/
				((total*sumvv-sumv*sumv)*(total*sumyy-sumy*sumy));

	free(x);
	if(prob_estimates != NULL)
		free(prob_estimates);
}

void exit_with_help()
{
	mexPrintf(
	"Usage: [predicted_label, accuracy, decision_values/prob_estimates] = svmpredict(testing_label_vector, testing_instance_matrix, model, 'libsvm_options')\n"
	"libsvm_options:\n"
	"-b probability_estimates: whether to predict probability estimates, 0 or 1 (default 0); one-class SVM not supported yet\n"
	);
}

void mexFunction( int nlhs, mxArray *plhs[],
		 int nrhs, const mxArray *prhs[] )
{
	int prob_estimate_flag = 0;
	struct svm_model *model;

	if(nrhs > 4 || nrhs < 3)
	{
		exit_with_help();
		fake_answer(plhs);
		return;
	}

	if(!mxIsDouble(prhs[0]) || !mxIsDouble(prhs[1])) {
		mexPrintf("Error: label vector and instance matrix must be double\n");
		fake_answer(plhs);
		return;
	}

	if(mxIsStruct(prhs[2]))
	{
		const char *error_msg;


		if(nrhs==4)
		{
			int i, argc = 1;
			char cmd[CMD_LEN], *argv[CMD_LEN/2];


			mxGetString(prhs[3], cmd,  mxGetN(prhs[3]) + 1);
			if((argv[argc] = strtok(cmd, " ")) != NULL)
				while((argv[++argc] = strtok(NULL, " ")) != NULL)
					;

			for(i=1;i<argc;i++)
			{
				if(argv[i][0] != '-') break;
				if(++i>=argc)
				{
					exit_with_help();
					fake_answer(plhs);
					return;
				}
				switch(argv[i-1][1])
				{
					case 'b':
						prob_estimate_flag = atoi(argv[i]);
						break;
					default:
						mexPrintf("Unknown option: -%c\n", argv[i-1][1]);
						exit_with_help();
						fake_answer(plhs);
						return;
				}
			}
		}

		model = matlab_matrix_to_model(prhs[2], &error_msg);
		if (model == NULL)
		{
			mexPrintf("Error: can't read model: %s\n", error_msg);
			fake_answer(plhs);
			return;
		}

		if(prob_estimate_flag)
		{
			if(svm_check_probability_model(model)==0)
			{
				mexPrintf("Model does not support probabiliy estimates\n");
				fake_answer(plhs);
				svm_destroy_model(model);
				return;
			}
		}
		else
		{
			if(svm_check_probability_model(model)!=0)
				printf("Model supports probability estimates, but disabled in predicton.\n");
		}

		predict(plhs, prhs, model, prob_estimate_flag);

		svm_destroy_model(model);
	}
	else
	{
		mexPrintf("model file should be a struct array\n");
		fake_answer(plhs);
	}

	return;
}
