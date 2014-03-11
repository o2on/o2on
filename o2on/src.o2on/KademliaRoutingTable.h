/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: KademliaRoutingTable.h
 * description	: 
 *
 */

#pragma once
#include "KademliaKBucket.h"

#define NEIGHBORS_LIMIT		3



template<class KB, class T>
class KademliaRoutingTable
{
protected:
	KB KBuckets[HASH_BITLEN];
	T SelfKademliaNode;

	virtual bool touch_preprocessor(T &node) = 0;

public:
    // NodeListT
	typedef std::list<T> NodeListT;

    // ソート用関数オブジェクト
	struct SortByDistancePred {
		const hashT &sourceID;
		SortByDistancePred(const hashT &id) : sourceID(id) {}
		bool operator()(const T &x, const T &y) const {
			return (less_xor_bitlength(sourceID, x.id, y.id));
//			hashBitsetT d1 = x.id.bits ^ sourceID.bits;
//			hashBitsetT d2 = y.id.bits ^ sourceID.bits;
//			return (d1 < d2);
		}
	};

	// -----------------------------------------------------------------------
    //  コンストラクタ・デストラクタ
    // -----------------------------------------------------------------------
	KademliaRoutingTable(void)
    {
		for (size_t i = 0; i < HASH_BITLEN; i++) {
			size_t c = i / 2;
			if (c < KADEMLIA_K)
				c = KADEMLIA_K;
			KBuckets[i].set_capacity(c);
		}
    }

    // -----------------------------------------------------------------------
    //  SetSelfID, SetSelfIP, SetSelfPort
    //  自身のノード情報をセット
    // -----------------------------------------------------------------------
	bool SetSelfNodeID(const hashT &id)
    {
        SelfKademliaNode.id = id;
        return (SelfKademliaNode.valid());
    }
	bool SetSelfNodeIP(const ulong ip)
    {
        SelfKademliaNode.ip = ip;
        return (SelfKademliaNode.valid());
    }
	bool SetSelfNodePort(const ushort port)
    {
        SelfKademliaNode.port = port;
        return (SelfKademliaNode.valid());
    }

    // -----------------------------------------------------------------------
    //  touch
    //  与えられたノード情報を適切なk-bucketへ追加
    // -----------------------------------------------------------------------
	bool touch(T &node)
    {
		if (!touch_preprocessor(node))
			return false;

        //hashBitsetT d = node.id.bits ^ SelfKademliaNode.id.bits;
        //int bitlen = d.bit_length() - 1;
		int bitlen = hash_xor_bitlength(node.id, SelfKademliaNode.id) - 1;

        if (bitlen < 0)
			return false; //myself

        return (KBuckets[bitlen].push(node));
    }
	
	// -----------------------------------------------------------------------
    //  remove
    //  ノードを削除
    // -----------------------------------------------------------------------
    void remove(const T &node)
    {
        //hashBitsetT d = node.id.bits ^ SelfKademliaNode.id.bits;
        //int bitlen = d.bit_length() - 1;
		int bitlen = hash_xor_bitlength(node.id, SelfKademliaNode.id) - 1;

        if (bitlen < 0)
			return; //myself

        KBuckets[bitlen].remove(node);
	}

    // -----------------------------------------------------------------------
    //  neighbors
    //  targetに近いノードをリストアップ
    // -----------------------------------------------------------------------
	size_t neighbors(hashT target, NodeListT &out, bool include_myself, uint limit = NEIGHBORS_LIMIT)
    {
//		SortByDistancePred comparetor(target);

//		hashBitsetT d = target.bits ^ SelfKademliaNode.id.bits;
//		int bitlen = d.bit_length() - 1;
		hashT d;
		hash_xor(d, target, SelfKademliaNode.id);
		int bitlen = hash_bitlength(d) - 1;

        out.clear();
        size_t index = 0;

		if (bitlen >= 0) {
			if (KBuckets[bitlen].count()) {
				//index = pick(out, limit, KBuckets[bitlen], comparetor);
				index = KBuckets[bitlen].pick(target, out, limit);
				if (index >= limit)
					return (out.size());
			}

			//target-自分間でXORビットの等しいノード追加
			for (int i = bitlen - 1; i >= 0; i--) {
				if (hash_bittest(d,i) && KBuckets[i].count()) {
					//index = pick(out, limit, KBuckets[i], comparetor);
					index = KBuckets[i].pick(target, out, limit);
					if (index >= limit)
						return (out.size());
				}
			}
		}

		//自身を追加
		if (include_myself && SelfKademliaNode.valid()) {
			out.push_back(SelfKademliaNode);
			index++;
			if (index >= limit)
				return (out.size());
		}

		if (bitlen >= 0) {
			//自分-target間でXORビットの異なるノード追加
			for (int i = 0; i < bitlen; i++) {
				if (!hash_bittest(d,i) && KBuckets[i].count()) {
					//index = pick(out, limit, KBuckets[i], comparetor);
					index = KBuckets[i].pick(target, out, limit);
					if (index >= limit)
						return (out.size());
				}
			}
		}

		//target-最長距離間のノード追加
		for (int i = bitlen + 1; i < HASH_BITLEN; i++) {
			if (KBuckets[i].count()) {
				//index = pick(out, limit, KBuckets[i], comparetor);
				index = KBuckets[i].pick(target, out, limit);
				if (index >= limit)
					return (out.size());
			}
		}

		return (out.size());
	}

    // -----------------------------------------------------------------------
    //  count
    //  
    // -----------------------------------------------------------------------
	size_t count(void)
	{
		size_t n = 0;
        for (size_t i = 0; i < HASH_BITLEN; i++) {
			n += KBuckets[i].count();
        }
		return (n);
	}

#if 0
private:
    // -----------------------------------------------------------------------
    //  pick
    //  kbのノードリストをdestに追加
    // -----------------------------------------------------------------------
	size_t pick(NodeListT &dest, size_t limit, KB &kb
		      , const SortByDistancePred &comparator)
    {
stopwatch sw("pick");
		NodeListT result;
        kb.get_nodes(result);
        if (result.empty())
            return (dest.size());

		std::sort(result.begin(), result.end(), comparator);

        if (result.size() + dest.size() > limit)
            dest.insert(dest.end(), result.begin(), result.begin() + (limit - dest.size()));
        else
            dest.insert(dest.end(), result.begin(), result.end());
		return (dest.size());
	}
#endif
};
