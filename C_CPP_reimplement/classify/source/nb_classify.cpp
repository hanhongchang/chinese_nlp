// Naive Bayes Implementation
// (c) Tim Nugent 2014
// timnugent@gmail.com

// Modified by taozhijiang@gmail.com

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cmath>
#include <sstream>
#include <vector>
#include <map>

#include <cmath>
#include <float.h>

#include "header.hpp"

#include <time.h>

using namespace std;

vector<string> &split(const string &s, char delim, vector<std::string> &elems) 
{
    stringstream ss(s);
    string item;
    while (std::getline(ss, item, delim)) 
    {
        elems.push_back(item);
    }
    return elems;
}

vector<string> split(const string &s, char delim) 
{
    vector<string> elems;
    split(s, delim, elems);
    return elems;
}

// 同时训练出伯努利分布和多项式分布的朴素贝叶斯分类器
bool train_classifyer(CLASS_DATA_STRUCT &cds, int BEST_N, double alpha, bool eval_mode)
{
    // For segment fault debug
    struct sigaction sa;
    sa.sa_handler = backtrace_info;
    sigaction(SIGSEGV, &sa, NULL); 

    unsigned int n_total = 0;       //文档总数
    map<int,int> n;                   //文档各个分类数
    map<int,vector<int> > sum_x;    //词个数按照标签统计
    map<int,int> multinomial_sums;

    cds.useful_words.clear();
    cds.bernoulli_means.clear();
    cds.priors.clear();
    cds.multinomial_likelihoods.clear();

    if (BEST_N > cds.sorted_wscores.size())
        BEST_N = cds.sorted_wscores.size();
    for(int i = 0; i < BEST_N; i++)
        cds.useful_words[cds.sorted_wscores[i]] = i;

    if(verbose)
        cout << "TRAIN STAGE - 1" << endl;

    //遍历 train_info
    map<int, vector< vector<int> > > :: iterator it;

    for (it = cds.train_info.begin(); it != cds.train_info.end(); it++)   //每一个标签
    {
        int tag_id = it->first;

        if (verbose)
            cout << "PROCESSING:" <<  cds.train_tags[tag_id] << "\tSIZE:" <<  it->second.size() << endl;

        vector<int> dummy_int;
        for (int i = 0; i<BEST_N; i++)
        {
            dummy_int.push_back(0);
        }
        sum_x[tag_id] = dummy_int; 
        n[tag_id] = 0;

        vector< vector<int> > t_items = it->second;  //训练文档

        for ( int i = 0; i< t_items.size(); i++ )
        {
            if ((i % 5000) == 0 and i > 0 and verbose)
                cout << "DOC NUM:" << i << endl;
            
            vector<int> t_item = t_items.at(i);     //文档中的词
            for (int j = 0; j < t_item.size(); j++)  //每个词
            {
                // 看该单词是不是在选定的特征词当中
                auto word_index = cds.useful_words.find(t_item[j]);
                if ( word_index != cds.useful_words.end())
                {
                    sum_x[tag_id][word_index->second] += 1;
                    multinomial_sums[tag_id] += 1;          //分类总的特征计数
                }
            }

            n_total += 1;       //文档总数
            n[tag_id] += 1;     //文档分类数
        }
    }

    if (verbose)
        cout << "TRAIN STAGE - 2" << endl;

    for(auto it = sum_x.begin(); it != sum_x.end(); it++)
    {
        int tag_id = it->first;
        cds.priors[tag_id] = (double)n[tag_id]/n_total;   //文档频率

        // Calculate means
        vector<double> feature_means;
        double cache_bls = 0.0f;
        for(unsigned int i = 0; i < BEST_N; i++)
        {
            feature_means.push_back(((double)sum_x[tag_id][i] + 1 )/(n[tag_id] + 2));
            cache_bls +=  -log(1 - feature_means[i]);
        }        

        // Calculate multinomial likelihoods
        for(unsigned int i = 0; i < BEST_N; i++)
        {
            double mnl = ((double)sum_x[tag_id][i]+alpha)/(multinomial_sums[tag_id]+(alpha*BEST_N));
            cds.multinomial_likelihoods[tag_id].push_back(mnl);
        }

        cds.bernoulli_means[tag_id] = feature_means;   
        cds.cache_bl[tag_id] = cache_bls;
    }

    if (verbose)
        cout << "TRAIN DONE, RELEASE TRAIN DATA!\n" << endl;

    if(!eval_mode)
        cds.train_info.clear();

    return true;
}

void eval_classifyers_and_args(CLASS_DATA_STRUCT &cds)
{
    
    cout << "TESTING CLASSIFIER!\n"<< endl;

    int best_ns[] = {2000, 4000, 6000, 8000, 10000, 14000, 16000, 20000, 25000, 300000};

    map<int, double> ret;

    for (int i=0; i< sizeof(best_ns)/sizeof(best_ns[0]); i++)
    {
        time_t t1 = time(NULL);
        ret.clear();
        eval_classifyer(cds, best_ns[i], ret);
        fprintf(stdout, "BEST_N:%d\tBL:%f\tMN:%f\tTIME:%ldsecs\n", best_ns[i], ret[BernoulliNB], ret[MultinomialNB],
            time(NULL) - t1);
    }
}

// 测试分类结果
static bool eval_classifyer(CLASS_DATA_STRUCT &cds, int BEST_N, map<int, double> &store)
{

#define FAST_MODE

    // train first
    train_classifyer(cds, BEST_N, 1, true);

    //遍历 test_info
    map<int, vector< vector<int> > > :: iterator it;

    int total_test = 0;
    int corr_test_bl = 0;
    int corr_test_mn = 0;

    for (it = cds.test_info.begin(); it != cds.test_info.end(); it++)   //每一个标签
    {
        int tag_id = it->first; 
        vector< vector<int> > t_items = it->second; //labled docs

        if (verbose)
            cout << "TESTING: " << cds.train_tags[tag_id] << endl;

        for(int it_ds = 0; it_ds < t_items.size(); it_ds++)             //每一个测试文档
        {
            vector<int> values = t_items[it_ds];
            int pred_id_bl = 0;
            int pred_id_mn = 0;
            double maxlikelihood_bl = FLT_MAX;
            double maxlikelihood_mn = FLT_MAX;
            double numer_bl = 0.0; 
            double numer_mn = 0.0;

            for(auto it_d = cds.priors.begin(); it_d != cds.priors.end(); it_d++)  //每一个文档概率
            {
                int tag_id_p = it_d->first;
#ifdef FAST_MODE
                numer_bl = cds.cache_bl[tag_id_p] + (-log(cds.priors[tag_id_p]));
#else
                numer_bl = -log(cds.priors[tag_id_p]); 
#endif  //FAST_MODE             
                numer_mn = -log(cds.priors[tag_id_p]); 
#if 1
                
                map<int, int > :: iterator it_u;
                for(it_u = cds.useful_words.begin(); it_u != cds.useful_words.end(); it_u ++)
                {
                    bool hit = false;
                    for(unsigned int k = 0; k < values.size(); k++)
                    {
                        if( it_u->first == values[k])
                        {
                            hit = true;
                            break;
                        }
                    }

#ifdef FAST_MODE
                    if(hit)
                    {
                        numer_bl +=  -log(cds.bernoulli_means[tag_id_p][it_u->second]);
                        numer_bl -=  -log(1 - cds.bernoulli_means[tag_id_p][it_u->second]);

                        numer_mn +=  -log(cds.multinomial_likelihoods[tag_id_p][it_u->second]);
                    }

#else
                    if(hit)
                    {
                        numer_bl +=  -log(cds.bernoulli_means[tag_id_p][it_u->second]);
                        numer_mn +=  -log(cds.multinomial_likelihoods[tag_id_p][it_u->second]);
                    }
                    else
                    {
                        numer_bl +=  -log(1 - cds.bernoulli_means[tag_id_p][it_u->second]);
                    }
#endif    //FAST_MODE                
                }

                if(numer_bl < maxlikelihood_bl)
                {
                    maxlikelihood_bl = numer_bl;
                    pred_id_bl = tag_id_p;
                }   

                if(numer_mn < maxlikelihood_mn)
                {
                    maxlikelihood_mn = numer_mn;
                    pred_id_mn = tag_id_p;

                }   
#else                
                if(decision == BernoulliNB)
                {
                    for(unsigned int j = 0; j < values.size(); j++)
                    {
                        auto word_index = cds.useful_words.find(values[j]);
                        if ( word_index != cds.useful_words.end())
                        {
                            numer *= pow(cds.multinomial_likelihoods[tag_id_p][word_index->second],1); 
                        }
                    }
                }
                else if (decision == MultinomialNB)
                {
                    map<int, int > :: iterator it_u;
                    for(it_u = cds.useful_words.begin(); it_u != cds.useful_words.end(); it_u ++)
                    {
                        bool hit = false;
                        for(unsigned int k = 0; k < values.size(); k++)
                        {
                            if( it_u->first == values[k])
                            {
                                hit = true;
                                break;
                            }
                        }

                        if(hit)
                            numer *= (cds.bernoulli_means[tag_id_p][it_u->second]);
                        else
                            numer *= (1 - cds.bernoulli_means[tag_id_p][it_u->second]);
                    }
                                    
                }
                else
                {
                    cerr << "Unsupported classifier:" << decision << endl;
                    exit(-1);    
                }

                if(numer > maxlikelihood)
                {
                    maxlikelihood = numer;
                    pred_id = tag_id_p;

                }   
                denom += numer;
                probs.push_back(numer);
#endif
            }

            total_test ++;
            if (pred_id_bl == tag_id)
                corr_test_bl ++;
            if (pred_id_mn == tag_id)
                corr_test_mn ++;

            //cout << maxlikelihood_bl << " AND " << maxlikelihood_mn  << " CACHE "<< cds.cache_bl[tag_id] << endl;
        }
    }

    if (verbose)
    {
        cout << "BL>>>GOOD:" << corr_test_bl << ", TOTAL:"<< total_test << ", ACCURACY:"<< ((double)corr_test_bl)/total_test * 100 << "%" <<endl;
        cout << "MN>>>GOOD:" << corr_test_mn << ", TOTAL:"<< total_test << ", ACCURACY:"<< ((double)corr_test_mn)/total_test * 100 << "%" <<endl;
    }

    store[BernoulliNB]   = (double)(corr_test_bl)/total_test * 100;
    store[MultinomialNB] = (double)(corr_test_mn)/total_test * 100;

#undef FAST_MODE

    return true;

}


/**
 * 返回值当中，0表示的是概率最大的标签，其余是按照tag_id罗列起来的
 */
bool predict_it(CLASS_DATA_STRUCT &cds, vector<std::string> str, 
    enum CLASSIFIER class_t, map<int, double> & store)
{
    double numer = 0.0f;
    double denom = 0.0f;
    int     pred_id = 0;
    vector<double> tmp_store;
    double maxlikelihood = FLT_MAX;

    for (int tag_id = 1; tag_id < cds.train_tags.size(); tag_id++)
    {
        if(class_t == MultinomialNB)
        {   

            numer = cds.priors[tag_id];

            for(int i=0; i<str.size(); i++)
            {
                auto word_num =  cds.train_w_id.find(str[i]);
                if (word_num == cds.train_w_id.end() )
                    continue;

                auto word_index = cds.useful_words.find(word_num->second);
                if ( word_index != cds.useful_words.end())
                {
                    numer *=  (cds.multinomial_likelihoods[tag_id][word_index->second]);
                }
            }

            // store it
            tmp_store.push_back(numer);
            denom += numer;
            //
            if(numer < maxlikelihood)
            {
                maxlikelihood = numer;
                pred_id = tag_id;
            } 
        }
        else if(class_t == BernoulliNB)
        {
            //numer = cds.cache_bl[tag_id] + (-log(cds.priors[tag_id]));
        }
        else
        {
            cerr << "Unknown Classify Type!" << endl;
            exit(-1);
        }
    }

    //store.clear();
    for (int tag_id = 1; tag_id < cds.train_tags.size(); tag_id++)
    {
       //store[tag_id] = tmp_store[tag_id-1] / denom;
        cout << tmp_store[tag_id-1]/denom << endl;
    }
    //store[0] = pred_id;

    return true;
    
}

