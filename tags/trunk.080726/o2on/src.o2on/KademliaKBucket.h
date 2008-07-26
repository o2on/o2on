/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: KademliaKBucket.h
 * description	: 
 *
 */

#pragma once
#include "KademliaNode.h"
#include "mutex.h"
#include <list>
#include <map>

#define KADEMLIA_K	20
#define USEPING		0




template<class T>
class KademliaKBucket
    : public Mutex
{
public:
	typedef std::list<T> NodeListT;

protected:
	NodeListT InternalList;
	size_t Capacity;
	virtual bool ping(const T &node) const = 0;

public:
	KademliaKBucket(void)
        : Capacity(KADEMLIA_K)
    {
    }

	~KademliaKBucket()
	{
	}

	bool set_capacity(size_t capacity)
	{
		if (capacity == 0)
			return false;
		Capacity = capacity;
		return true;
	}

	void clear(void)
    {
        Lock();
        InternalList.clear();
        Unlock();
    }

    size_t count(void)
    {
        Lock();
        size_t ret = InternalList.size();
        Unlock();
        return (ret);
    }

    bool pop_front(void)
    {
        bool ret = false;
        Lock();
        if (!InternalList.empty()) {
            InternalList.pop_front();
            ret = true;
        }
        Unlock();
        return (ret);
    }

    bool get_front(T &node)
    {
        bool ret = false;
        Lock();
        if (!InternalList.empty()) {
            node = InternalList.front();
            ret = true;
        }
        Unlock();
        return (ret);
    }

	void remove(const T &node)
	{
		NodeListT::iterator it;
        Lock();
        {
			it = std::find(
				InternalList.begin(), InternalList.end(), node);

			if (it != InternalList.end())
				InternalList.erase(it);
		}
		Unlock();
	}

    bool push(const T &node)
    {
        if (!node.valid())
            return false;

        bool proceeded = false;
        Lock();
        {
			NodeListT::iterator it;
            it = std::find(
                InternalList.begin(), InternalList.end(), node);

            if (it != InternalList.end()) {
                //既存なら最後尾へ
				T mnode(*it);
                InternalList.erase(it);

				mnode.marge(node);
				mnode.lastlink = time(NULL);
				InternalList.push_back(mnode);
                proceeded = true;
            }
            else if (InternalList.size() < Capacity) {
                //リストに余裕があるなら最後尾に追加
				T newnode(node);
				newnode.lastlink = time(NULL);
                InternalList.push_back(newnode);
                proceeded = true;
            }
#if !USEPING
			else {
				pop_front();
				T newnode(node);
				newnode.lastlink = time(NULL);
				InternalList.push_back(newnode);
                proceeded = true;
			}
#endif
		}
        Unlock();

#if USEPING
        if (proceeded)
            return true;

        //リストの一番上のノード＝最古ノードの生存確認
        T head;
        get_front(head);
		bool alive = ping(head);

        if (alive) {
            //最古ノードが生きている：最古ノードを最後尾へ
            //※対象ノードは追加しない
            pop_front();
			head.lastlink = time(NULL);
            Lock();
            InternalList.push_back(head);
            Unlock();
        }
        else {
            //最古ノードが死んでいる：対象ノードを最後尾に追加
            pop_front();
			T newnode(node);
			newnode.lastlink = time(NULL);
            Lock();
            InternalList.push_back(newnode);
            Unlock();
        }
#endif
		return true;
    }

    size_t get_nodes(NodeListT &out)
    {
        Lock();
        out = InternalList;
        Unlock();
        return (out.size());
    }

	size_t pick(const hashT &target, NodeListT &dest, size_t limit)
    {
//stopwatch sw("pick");
		typedef std::map<hashT,T*> SortedNodePListT; 
		SortedNodePListT sorted_list;

        Lock();
		{
			NodeListT::iterator it = InternalList.begin();
			for ( ; it != InternalList.end(); it++) {
				hashT xor;
				hash_xor(xor, target, it->id);
				sorted_list.insert(SortedNodePListT::value_type(xor, &(*it)));
			}
			size_t i = 0;
			SortedNodePListT::iterator sit = sorted_list.begin();
			for ( ; sit != sorted_list.end() && i < limit; sit++, i++) {
				dest.push_back(*(sit->second));
			}
		}
        Unlock();

		return (dest.size());
	}
};
