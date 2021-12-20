#include "error.h"
using namespace std;

int getbit(vector<int> msg,int line, int bit)
{
    int value=msg[line];
    int bitvalue=0;
    for(int i =1; i<bit; i++)
    {
        value=value/2 ;
    }
    if(value%2==0)
    {
        bitvalue=0;
    }else{
        bitvalue=1;
    }
    return bitvalue;
}

int signal_to_int(std::vector<char> msg)
{
    vector<char> input;
    vector<int> t;
    input=msg;
    for(int i =0;i<input.size();i++)
    {
        switch(input[i])
        {
            case('1'):
                t.push_back(0);
                break;

            case('2'):
                t.push_back(1);
                break;

            case('3'):
                t.push_back(2);
                break;

            case('a'):
                t.push_back(3);
                break;

            case('4'):
                t.push_back(4);
                break;

            case('5'):
                t.push_back(5);
                break;

            case('6'):
                t.push_back(6);
                break;

            case('b'):
                t.push_back(7);
                break;

            case('7'):
                t.push_back(8);
                break;

            case('8'):
                t.push_back(9);
                break;

            case('9'):
                t.push_back(10);
                break;

            case('c'):
                t.push_back(11);
                break;

            case('*'):
                t.push_back(12);
                break;

            case('0'):
                t.push_back(13);
                break;

            case('#'):
                t.push_back(14);
                break;

            case('d'):
                t.push_back(15);
                break;
        }
    }
    if(errorcheck(t)==true){
        if(getstop(t)==0){
            return getdir(t);
            std::cout << "getDir: " << getdir(t) << std::endl;
        }
        else{
            return getstop(t);
            std::cout << "getStop: " << getstop(t) << std::endl;
        }
    }
    return 0;
}

bool errorcheck(std::vector<int> msg)
{
    std::vector<int> t=msg;

    int pt=0,p1=0,p2=0,p3=0,p4=0;
    for(int i=0; i<t.size(); i++)
    {
        int a=0, b=0, c=0, d=0;
        a=getbit(t,i,1);
        b=getbit(t,i,2);
        c=getbit(t,i,3);
        d=getbit(t,i,4);
        pt=pt+a+b+c+d;
        p1=p1+c+a;
        p2=p2+a+b;
        if(i==1||i==3)
        {
            p3=p3+a+b+c+d;
        }
        if(i==2||i==3)
        {
            p4=p4+a+b+c+d;
        }
    }
//fejl retning
    if(pt%2!=0 || p1%2!=0 || p2%2!=0 || p3%2!=0 || p4%2!=0 )
    {

        //ret 1 bit fejl
        if(p1%2==1 && p2%2==0 && p3%2==0 && p4%2==0)
        {
            if(getbit(t,0,3)==1){
                t.at(0)=t.at(0)-0b0100;
            }else{
                t.at(0)=t.at(0)+0b0100;
            }
        }else if(p1%2==0 && p2%2==1 && p3%2==0 && p4%2==0)
        {
            if(getbit(t,0,2)==1){
                t.at(0)=t.at(0)-0b0010;
            }else{
                t.at(0)=t.at(0)+0b0010;
            }
        }else if(p1%2==1 && p2%2==1 && p3%2==0 && p4%2==0)
        {
            if(getbit(t,0,1)==1){
                t.at(0)=t.at(0)-0b0001;
            }else{
                t.at(0)=t.at(0)+0b0001;
            }
        }else if(p1%2==0 && p2%2==0 && p3%2==1 && p4%2==0)
        {
            if(getbit(t,1,4)==1){
                t.at(1)=t.at(1)-0b1000;
            }else{
                t.at(1)=t.at(1)+0b1000;
            }
        }else if(p1%2==1 && p2%2==0 && p3%2==1 && p4%2==0)
        {
            if(getbit(t,1,3)==1){
                t.at(1)=t.at(1)-0b0100;
            }else{
                t.at(1)=t.at(1)+0b0100;
            }
        }else if(p1%2==0 && p2%2==1 && p3%2==1 && p4%2==0)
        {
            if(getbit(t,1,2)==1){
                t.at(1)=t.at(1)-0b0010;
            }else{
                t.at(1)=t.at(1)+0b0010;
            }
        }else if(p1%2==1 && p2%2==1 && p3%2==1 && p4%2==0)
        {
            if(getbit(t,1,1)==1){
                t.at(1)=t.at(1)-0b0001;
            }else{
                t.at(1)=t.at(1)+0b0001;
            }
        }else if(p1%2==0 && p2%2==0 && p3%2==0 && p4%2==1)
        {
            if(getbit(t,2,4)==1){
                t.at(2)=t.at(2)-0b1000;
            }else{
                t.at(2)=t.at(2)+0b1000;
            }
        }else if(p1%2==1 && p2%2==0 && p3%2==0 && p4%2==1)
        {
            if(getbit(t,2,3)==1){
                t.at(2)=t.at(2)-0b0100;
            }else{
                t.at(2)=t.at(2)+0b0100;
            }
        }else if(p1%2==0 && p2%2==1 && p3%2==0 && p4%2==1)
        {
            if(getbit(t,2,2)==1){
                t.at(2)=t.at(2)-0b0010;
            }else{
                t.at(2)=t.at(2)+0b0010;
            }
        }else if(p1%2==1 && p2%2==1 && p3%2==0 && p4%2==1)
        {
            if(getbit(t,2,1)==1){
                t.at(2)=t.at(2)-0b0001;
            }else{
                t.at(2)=t.at(2)+0b0001;
            }
        }else if(p1%2==0 && p2%2==0 && p3%2==1 && p4%2==1)
        {
            if(getbit(t,3,4)==1){
                t.at(3)=t.at(3)-0b1000;
            }else{
                t.at(3)=t.at(3)+0b1000;
            }
        }else if(p1%2==1 && p2%2==0 && p3%2==1 && p4%2==1)
        {
            if(getbit(t,3,3)==1){
                t.at(3)=t.at(3)-0b0100;
            }else{
                t.at(3)=t.at(3)+0b0100;
            }
        }else if(p1%2==0 && p2%2==1 && p3%2==1 && p4%2==1)
        {
            if(getbit(t,3,2)==1){
                t.at(3)=t.at(3)-0b0010;
            }else{
                t.at(3)=t.at(3)+0b0010;
            }
        }else if(p1%2==1 && p2%2==1 && p3%2==1 && p4%2==1)
        {
            if(getbit(t,3,1)==1){
                t.at(3)=t.at(3)-0b0001;
            }else{
                t.at(3)=t.at(3)+0b0001;
            }
        }
        pt=0;
        p1=0;
        p2=0;
        p3=0;
        p4=0;
        for(int i=0; i<t.size(); i++)
        {
            int a=0, b=0, c=0, d=0;
            a=getbit(t,i,1);
            b=getbit(t,i,2);
            c=getbit(t,i,3);
            d=getbit(t,i,4);
            pt=pt+a+b+c+d;
            p1=p1+c+a;
            p2=p2+a+b;
            if(i==1||i==3)
            {
                p3=p3+a+b+c+d;
            }
            if(i==2||i==3)
            {
                p4=p4+a+b+c+d;
            }
        }
    }
    if(pt%2!=0 || p1%2!=0 || p2%2!=0 || p3%2!=0 || p4%2!=0 )
    {
        return false;
    }else {
        return true;
    }
}

int getdir(std::vector<int> msg)
{
    return getbit(msg,1,1)+2*getbit(msg,1,2)+4*getbit(msg,1,3);
}

int getstop(std::vector<int> msg)
{
    return getbit(msg,0,1);
}