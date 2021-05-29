#include "delegate.h"
class CMultiDelegate
{
public:
    typedef std::list<IDelegate*> ListDelegate;
    typedef ListDelegate::iterator ListDelegateIterator;
//const　iterator
    typedef ListDelegate::const_iterator ConstListDelegateIterator;
    CMultiDelegate () { }
    ~CMultiDelegate () { clear(); }
//判断是否为空　对链表内的内容逐个判断
    bool empty() const
    {
        for (ConstListDelegateIterator iter = mListDelegates.begin(); iter!=mListDelegates.end(); ++iter)
        {
            if (*iter) return false;
        }
        return true;
    }
//对链表内的内容　进行逐个删除
    void clear()
    {
        for (ListDelegateIterator iter=mListDelegates.begin(); iter!=mListDelegates.end(); ++iter)
        {
            if (*iter)
            {
                delete (*iter);
                (*iter) = 0;
            }
        }
    }
//往链表中添加函数
    CMultiDelegate& operator+=(IDelegate* _delegate)
    {
//首先需要逐个判断，确定这个函数不在链表中，最后再向链表中添加
        for (ListDelegateIterator iter=mListDelegates.begin(); iter!=mListDelegates.end(); ++iter)
        {
            if ((*iter) && (*iter)->compare(_delegate))
            {
                delete _delegate;//需要将外部这个对象释放掉
                return *this;
            }
        }
        mListDelegates.push_back(_delegate);
        return *this;
    }
//从链表中删除掉这个函数
    CMultiDelegate& operator-=(IDelegate* _delegate)
    {
        for (ListDelegateIterator iter=mListDelegates.begin(); iter!=mListDelegates.end(); ++iter)
        {
            if ((*iter) && (*iter)->compare(_delegate))
            {
                if ((*iter) != _delegate) delete (*iter);
                (*iter) = 0;
                break;
            }
        }
        delete _delegate;
        return *this;
    }
//触发函数
    void operator()( )
    {
        ListDelegateIterator iter = mListDelegates.begin();
        while (iter != mListDelegates.end())
        {
            if (0 == (*iter))
            {
                iter = mListDelegates.erase(iter);
            }
            else
            {
                (*iter)->invoke();
                ++iter;
            }
        }
    }
private:
    CMultiDelegate (const CMultiDelegate& _event);
    CMultiDelegate& operator=(const CMultiDelegate& _event);
private:
    ListDelegate mListDelegates;
};
