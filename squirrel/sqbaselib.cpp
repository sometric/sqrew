/*
	see copyright notice in squirrel.h
*/
#include "sqpcheader.h"
#include "sqvm.h"
#include "sqstring.h"
#include "sqtable.h"
#include "sqarray.h"
#include "sqfuncproto.h"
#include "sqclosure.h"
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

int sq_aux_checkargs(HSQUIRRELVM v,int nargs,int ncheck,...)
{
	if(sq_gettop(v)<nargs)return sq_throwerror(v,_SC("invalid num of args"));
	int count = 0, sum = 0;
	SQObjectType type;
	va_list marker;

	va_start( marker, ncheck );     /* Initialize variable arguments. */
	for(int i=0;i<ncheck;i++){
		type = va_arg( marker, SQObjectType);
		if(sq_gettype(v,i+1)!=type)return sq_throwerror(v,_SC("invalid num of args"));
	}
	va_end( marker );              /* Reset variable arguments.      */
	return 1;
}

bool str2num(const SQChar *s,SQObjectPtr &res)
{
	SQChar *end;
	if(scstrstr(s,_SC("."))){
		SQFloat r=SQFloat(scstrtod(s,&end));
		if(s==end) return false;
		while (scisspace((*end)) ) end++;
		if (*end != _SC('\0')) return false;
		res = r;
		return true;
	}
	else{
		const SQChar *t=s;
		while(*t!=_SC('\0'))if(!scisdigit(*t++))return false;
		res=SQInteger(scatoi(s));
		return true;
	}
}

#ifdef GARBAGE_COLLECTOR
static int base_collect_garbage(HSQUIRRELVM v)
{
	sq_pushinteger(v,_ss(v)->CollectGarbage(v));
	return 1;
}
#endif

static int base_getroottable(HSQUIRRELVM v)
{
	v->Push(v->_roottable);
	return 1;
}

static int base_setroottable(HSQUIRRELVM v)
{
	if(SQ_FAILED(sq_aux_checkargs(v,2,0)))return -1;
	SQObjectPtr &o=stack_get(v,2);
	sq_setroottable(v);
	v->Push(o);
	return 1;
}

static int base_seterrorhandler(HSQUIRRELVM v)
{
	sq_seterrorhandler(v);
	return 0;
}

static int base_setdebughook(HSQUIRRELVM v)
{
	sq_setdebughook(v);
	return 0;
}

static int base_assert(HSQUIRRELVM v)
{
	if(sq_gettop(v)==2){
		if(sq_gettype(v,-1)!=OT_NULL){
			return 0;
		}
		else return sq_throwerror(v,_SC("assertion failed"));
	}
	return sq_throwerror(v,_SC("wrong number of params"));
}

static int get_slice_params(HSQUIRRELVM v,int &sidx,int &eidx,SQObjectPtr &o)
{
	int top;
	if((top=sq_gettop(v))>=2){
		sidx=0;
		eidx=0;
		o=stack_get(v,1);
		SQObjectPtr &start=stack_get(v,2);
		if(type(start)!=OT_NULL && sq_isnumeric(start)){
			sidx=tointeger(start);
		}
		if(top>2){
			SQObjectPtr &end=stack_get(v,3);
			if(sq_isnumeric(end)){
				eidx=tointeger(end);
			}
		}
		return 1;
	}return sq_throwerror(v,_SC("wrong number of params"));
}

static int base_print(HSQUIRRELVM v)
{
	if(sq_gettop(v)>=2){
		SQObjectPtr &o=stack_get(v,2);
		switch(type(o)){
		case OT_STRING:
			scprintf(_SC("%s"),_stringval(o));
			break;
		case OT_INTEGER:
			scprintf(_SC("%d"),_integer(o));
			break;
		case OT_FLOAT:
			scprintf(_SC("%.14g"),_float(o));
			break;
		default:{
			SQObjectPtr tname;
			v->TypeOf(o,tname);
			scprintf(_SC("(%s)"),_stringval(tname));
			}
			break;
		}
		return 0;
	}
	return sq_throwerror(v,_SC("wrong number of params"));
}

static int base_chcode2string(HSQUIRRELVM v)
{
	if(sq_gettop(v)>=2){
		SQObject &o=stack_get(v,2);
		if(type(o)&SQOBJECT_NUMERIC){
			SQChar c=tointeger(o);
			v->Push(SQString::Create(_ss(v),(const SQChar *)&c,1));
			return 1;
		}return sq_throwerror(v,_SC("has to be a number"));
	}
	return sq_throwerror(v,_SC("wrong number of params"));
}


static SQRegFunction base_funcs[]={
	//generic
	{_SC("seterrorhandler"),base_seterrorhandler},
	{_SC("setdebughook"),base_setdebughook},
	{_SC("getroottable"),base_getroottable},
	{_SC("setroottable"),base_setroottable},
	{_SC("assert"),base_assert},
	{_SC("print"),base_print},
	//string stuff
	{_SC("chcode2string"),base_chcode2string},
#ifdef GARBAGE_COLLECTOR
	{_SC("collect_garbage"),base_collect_garbage},
#endif
	{0,0}
};

void sq_base_register(HSQUIRRELVM v)
{
	int i=0;
	sq_pushroottable(v);
	while(base_funcs[i].name!=0){
		sq_pushstring(v,base_funcs[i].name,-1);
		sq_newclosure(v,base_funcs[i].f,0);
		sq_createslot(v,-3);
		i++;
	}
	sq_pop(v,1);
}

static int default_delegate_len(HSQUIRRELVM v)
{
	v->Push(SQInteger(sq_getsize(v,1)));
	return 1;
}

static int default_delegate_tofloat(HSQUIRRELVM v)
{
	if(sq_gettop(v)==1){
		SQObjectPtr &o=stack_get(v,1);
		switch(type(o)){
		case OT_STRING:{
			SQObjectPtr res;
			if(str2num(_stringval(o),res)){
				v->Push(SQObjectPtr(tofloat(res)));
				break;
			}}
		case OT_INTEGER:case OT_FLOAT:
			v->Push(SQObjectPtr(tofloat(o)));
			break;
		default:
			v->Push(_null_);
			break;
		}
		return 1;
	}
	return sq_throwerror(v,_SC("wrong number of params"));
}

static int default_delegate_tointeger(HSQUIRRELVM v)
{
	if(sq_gettop(v)==1){
		SQObjectPtr &o=stack_get(v,1);
		switch(type(o)){
		case OT_STRING:{
			SQObjectPtr res;
			if(str2num(_stringval(o),res)){
				v->Push(SQObjectPtr(tointeger(res)));
				break;
			}}
		case OT_INTEGER:case OT_FLOAT:
			v->Push(SQObjectPtr(tointeger(o)));
			break;
		default:
			v->Push(_null_);
			break;
		}
		return 1;
	}
	return sq_throwerror(v,_SC("wrong number of params"));
}

static int default_delegate_tostring(HSQUIRRELVM v)
{
	if(sq_gettop(v)==1){
		SQObjectPtr &o=stack_get(v,1);
		switch(type(o)){
		case OT_STRING:
			v->Push(o);
			break;
		case OT_INTEGER:
			scsprintf(_ss(v)->GetScratchPad(rsl(NUMBER_MAX_CHAR+1)),_SC("%d"),_integer(o));
			v->Push(SQString::Create(_ss(v),_ss(v)->GetScratchPad(-1)));
			break;
		case OT_FLOAT:
			scsprintf(_ss(v)->GetScratchPad(rsl(NUMBER_MAX_CHAR+1)),_SC("%.14g"),_float(o));
			v->Push(SQString::Create(_ss(v),_ss(v)->GetScratchPad(-1)));
			break;
		default:
			v->Push(_null_);
			break;
		}
		return 1;
	}
	return sq_throwerror(v,_SC("wrong number of params"));
}

/////////////////////////////////////////////////////////////////
//TABLE DEFAULT DELEGATE

static int table_getdelegate(HSQUIRRELVM v)
{
	return sq_getdelegate(v,-1);
}


static int table_rawset(HSQUIRRELVM v)
{
	return sq_rawset(v,-3);
}


static int table_rawget(HSQUIRRELVM v)
{
	return sq_rawget(v,-2);
}

SQRegFunction SQSharedState::_table_default_delegate_funcz[]={
	{_SC("len"),default_delegate_len},
	{_SC("rawget"),table_rawget},
	{_SC("rawset"),table_rawset},
	{_SC("getdelegate"),table_getdelegate},
	{0,0}
};

//ARRAY DEFAULT DELEGATE///////////////////////////////////////

static int array_append(HSQUIRRELVM v)
{
	return sq_arrayappend(v,-2);
}

static int array_extend(HSQUIRRELVM v)
{
	if(SQ_FAILED(sq_aux_checkargs(v,2,2,OT_ARRAY,OT_ARRAY)))return -1;
	_array(stack_get(v,1))->Extend(_array(stack_get(v,2)));
	return 0;
}

static int array_reverse(HSQUIRRELVM v)
{
	return sq_arrayreverse(v,-1);
}

static int array_pop(HSQUIRRELVM v)
{
	return sq_arraypop(v,1,1);
}

static int array_top(HSQUIRRELVM v)
{
	if(SQ_FAILED(sq_aux_checkargs(v,1,1,OT_ARRAY)))return -1;
	SQObject &o=stack_get(v,1);
	if(_array(o)->Size()>0){
		v->Push(_array(o)->Top());
		return 1;
	}
	else return sq_throwerror(v,_SC("top() on a empty array"));
}

static int array_insert(HSQUIRRELVM v)
{
	if(SQ_FAILED(sq_aux_checkargs(v,3,1,OT_ARRAY)))return -1;
	SQObject &o=stack_get(v,1);
	SQObject &idx=stack_get(v,2);
	SQObject &val=stack_get(v,3);
	_array(o)->Insert(idx,val);
	return 0;
}

static int array_remove(HSQUIRRELVM v)
{
	if(SQ_FAILED(sq_aux_checkargs(v,2,1,OT_ARRAY)))return -1;
	SQObject &o=stack_get(v,1);
	SQObject &idx=stack_get(v,2);
	if(!sq_isnumeric(idx))return sq_throwerror(v,_SC("wrong type"));
	SQObjectPtr val;
	if(_array(o)->Get(tointeger(idx),val)){
		_array(o)->Remove(idx);
		v->Push(val);
		return 1;
	}
	return sq_throwerror(v,_SC("idx out of range"));
}

static int array_resize(HSQUIRRELVM v)
{
	if(SQ_FAILED(sq_aux_checkargs(v,2,1,OT_ARRAY)))return -1;
	SQObject &o=stack_get(v,1);
	SQObject &nsize=stack_get(v,2);
	if(sq_isnumeric(nsize)){
		_array(o)->Resize(tointeger(nsize));
		return 0;
	}
	return sq_throwerror(v,_SC("size must be a number"));
}

//QSORT ala Sedgewick
void _qsort(HSQUIRRELVM v,SQArray &a, int l, int r)
{
	int i, j;
	SQObjectPtr pivot,t;
	if( l < r ){
		pivot = a._values[l];
		i = l; j = r+1;
		while(1){
			do ++i; while((i <= r) && (v->ObjCmp(a._values[i],pivot) <= 0));
			do --j; while( v->ObjCmp(a._values[j],pivot) > 0 );
			if( i >= j ) break;
			t = a._values[i]; a._values[i] = a._values[j]; a._values[j] = t;
		}
		t = a._values[l]; a._values[l] = a._values[j]; a._values[j] = t;
		_qsort( v, a, l, j-1);
		_qsort( v, a, j+1, r);
	}
}

static int array_sort(HSQUIRRELVM v)
{
	if(SQ_FAILED(sq_aux_checkargs(v,1,1,OT_ARRAY)))return -1;
	SQObject &o=stack_get(v,1);
	if(_array(o)->Size()>1){
		_qsort(v,*_array(o),0,_array(o)->Size()-1);
	}
	return 0;
}

static int array_slice(HSQUIRRELVM v)
{
	int sidx,eidx;
	SQObjectPtr o;
	if(SQ_FAILED(sq_aux_checkargs(v,1,1,OT_ARRAY)))return -1;
	if(get_slice_params(v,sidx,eidx,o)==-1)return -1;
	if(sidx<0)sidx=_array(o)->Size()+sidx;
	if(eidx<1)eidx=_array(o)->Size()+eidx;
	if(eidx<=sidx)return sq_throwerror(v,_SC("wrog indexes"));
	SQArray *arr=SQArray::Create(_ss(v),eidx-sidx);
	SQObjectPtr t;
	int count=0;
	for(int i=sidx;i<eidx;i++){
		_array(o)->Get(i,t);
		arr->Set(count++,t);
	}
	v->Push(arr);
	return 1;
	
}

SQRegFunction SQSharedState::_array_default_delegate_funcz[]={
	{_SC("len"),default_delegate_len},
	{_SC("append"),array_append},
	{_SC("extend"),array_extend},
	{_SC("push"),array_append},
	{_SC("pop"),array_pop},
	{_SC("top"),array_top},
	{_SC("insert"),array_insert},
	{_SC("remove"),array_remove},
	{_SC("resize"),array_resize},
	{_SC("reverse"),array_reverse},
	{_SC("sort"),array_sort},
	{_SC("slice"),array_slice},
	{0,0}
};

//STRING DEFAULT DELEGATE//////////////////////////
static int string_slice(HSQUIRRELVM v)
{
	int sidx,eidx;
	SQObjectPtr o;
	if(SQ_FAILED(get_slice_params(v,sidx,eidx,o)))return -1;
	if(sidx<0)sidx=_string(o)->_len+sidx;
	if(eidx<1)eidx=_string(o)->_len+eidx;
	if(eidx<=sidx)return sq_throwerror(v,_SC("wrog indexes"));
	v->Push(SQString::Create(_ss(v),&_stringval(o)[sidx],eidx-sidx));
	return 1;
}

#define STRING_TOFUNCZ(func) static int string_##func(HSQUIRRELVM v) \
{ \
	if(SQ_FAILED(sq_aux_checkargs(v,1,1,OT_STRING)))return -1; \
	SQObject str=stack_get(v,1); \
	int len=_string(str)->_len; \
	const SQChar *sThis=_stringval(str); \
	SQChar *sNew=(_ss(v)->GetScratchPad(rsl(len))); \
	for(int i=0;i<len;i++) sNew[i]=func(sThis[i]); \
	v->Push(SQString::Create(_ss(v),sNew,len)); \
	return 1; \
}


STRING_TOFUNCZ(tolower)
STRING_TOFUNCZ(toupper)

SQRegFunction SQSharedState::_string_default_delegate_funcz[]={
	{_SC("len"),default_delegate_len},
	{_SC("tointeger"),default_delegate_tointeger},
	{_SC("tofloat"),default_delegate_tofloat},
	{_SC("slice"),string_slice},
	{_SC("tolower"),string_tolower},
	{_SC("toupper"),string_toupper},
	{0,0}
};

//INTEGER DEFAULT DELEGATE//////////////////////////
SQRegFunction SQSharedState::_number_default_delegate_funcz[]={
	{_SC("tointeger"),default_delegate_tointeger},
	{_SC("tofloat"),default_delegate_tofloat},
	{_SC("tostring"),default_delegate_tostring},
	{0,0}
};

//CLOSURE DEFAULT DELEGATE//////////////////////////
static int closure_call(HSQUIRRELVM v)
{
	return sq_call(v,sq_gettop(v)-1,1);
}

static int closure_acall(HSQUIRRELVM v)
{
	if(SQ_FAILED(sq_aux_checkargs(v,2,2,OT_CLOSURE,OT_ARRAY)))return -1;
	SQArray *aparams=_array(stack_get(v,2));
	int nparams=aparams->Size();
	v->Push(stack_get(v,1));
	for(int i=0;i<nparams;i++)v->Push(aparams->_values[i]);
	return sq_call(v,nparams,1);
}

SQRegFunction SQSharedState::_closure_default_delegate_funcz[]={
	{_SC("call"),closure_call},
	{_SC("acall"),closure_acall},
	{0,0}
};

//GENERATOR DEFAULT DELEGATE
static int generator_getstatus(HSQUIRRELVM v)
{
	if(SQ_FAILED(sq_aux_checkargs(v,1,1,OT_GENERATOR)))return -1;
	SQObject &o=stack_get(v,1);
	switch(_generator(o)->_state){
		case SQGenerator::eSuspended:v->Push(SQString::Create(_ss(v),_SC("suspended")));break;
		case SQGenerator::eRunning:v->Push(SQString::Create(_ss(v),_SC("running")));break;
		case SQGenerator::eDead:v->Push(SQString::Create(_ss(v),_SC("dead")));break;
	}
	return 1;
}

SQRegFunction SQSharedState::_generator_default_delegate_funcz[]={
	{_SC("getstatus"),generator_getstatus},
	{0,0}
};
