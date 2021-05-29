#pragma once
class IDelegate
{
public:
    virtual ~IDelegate() { }
    virtual bool isType(const std::type_info& _type) = 0;
　　//用于触发函数
    virtual void invoke() = 0;
    virtual bool compare(IDelegate *_delegate) const = 0;
};
//封装普通函数指针
class CStaticDelegate : public IDelegate
{
public:
    typedef void (*Func)();
    CStaticDelegate(Func _func) : mFunc(_func) { }
    virtual bool isType( const std::type_info& _type) { return typeid(CStaticDelegate) == _type; }
    virtual void invoke() { mFunc(); }
    virtual bool compare(IDelegate *_delegate) const
    {
        if (0 == _delegate || !_delegate->isType(typeid(CStaticDelegate)) ) return false;
        CStaticDelegate * cast = static_cast<CStaticDelegate*>(_delegate);
        return cast->mFunc == mFunc;
    }
private:
    Func mFunc;
};
//封装类成员函数
//一个类实例指针以及类成员函数的指针
template<class T>
class CMethodDelegate : public IDelegate
{
public:
    typedef void (T::*Method)();
    CMethodDelegate(T * _object, Method _method) : mObject(_object), mMethod(_method) { }
    virtual bool isType( const std::type_info& _type) { return typeid(CMethodDelegate) == _type; }
    virtual void invoke()
    {
        (mObject->*mMethod)();
    }
    virtual bool compare(IDelegate *_delegate) const
    {
        if (0 == _delegate || !_delegate->isType(typeid(CMethodDelegate)) ) return false;
        CMethodDelegate* cast = static_cast<CMethodDelegate* >(_delegate);
        return cast->mObject == mObject && cast->mMethod == mMethod;
    }
private:
    T * mObject;
    Method mMethod;
};

//定义函数newDelegate来创建委托使用的函数
inline IDelegate* newDelegate( void (*_func)() )
{
    return new CStaticDelegate(_func);
}
template<class T>
inline IDelegate* newDelegate( T * _object, void (T::*_method)() )
{
    return new CMethodDelegate<T>(_object, _method);
}
