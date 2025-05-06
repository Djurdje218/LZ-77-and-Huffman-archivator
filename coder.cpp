

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <queue>
#include <unordered_map>
#include <cstdint>
#include <cstring>

static const size_t WINDOW_SIZE = 32768;
static const int    DIST_BITS   = 15;
static const size_t HASH_SIZE   = 1<<16;   
static const int    MIN_MATCH   = 3;       
static const int    MAX_CHAIN   = 64;      


class BitWriter {
    
    std::ofstream &out;
    uint8_t buf = 0;
    int n = 0;
public:
    BitWriter(std::ofstream &o): out(o) {}
    inline void writeBit(int b) {

        buf = (uint8_t)((buf<<1)|(b&1));

        if (++n == 8) {
            out.put(buf);
            buf = n = 0;
        }
            }
    inline void writeBitsUInt(uint32_t v, int len) {

        for (int i = len-1; i >= 0; --i) writeBit((v >> i) & 1);
    }

        void flush() {              
            if (n) {
                buf <<= (8 - n);
                out.put(buf);
                buf = 0;
                n   = 0;
            }
        }
        
};

struct Code {
     uint32_t bits; uint8_t len; 
    };

static std::string toBinary(uint32_t v, int bits) {

    std::string s(bits, '0');
    for (int i = bits-1; i >= 0; --i)  s[i] = char('0' + (v & 1)); v >>= 1; 

    return s;
}


struct HNode {
    int symbol; uint64_t freq;
    HNode *l,*r;
    
    HNode(int s, uint64_t f): symbol(s), freq(f), l(nullptr), r(nullptr) {}
    HNode(HNode* L, HNode* R): symbol(-1), freq(L->freq+R->freq), l(L), r(R) {}
};

struct HCmp {
     bool operator()(HNode* a, HNode* b) const { return a->freq > b->freq; } 
    };

void buildCodes(HNode* node, uint32_t code, uint8_t depth,std::unordered_map<int,Code> &table) {
    if (!node->l && !node->r){

        table[node->symbol] = { code, depth };
        return;
    }
    if (node->l) buildCodes(node->l, (code<<1)|0, depth+1, table);
    if (node->r) buildCodes(node->r, (code<<1)|1, depth+1, table);
}

void deleteTree(HNode* node) {
    if (!node) return;
    deleteTree(node->l);
    deleteTree(node->r);
    delete node;
}

int main(int argc, char** argv) {

    if (argc != 3)
    {
        std::cerr << "Usage -> encoder 'in.txt' 'out.lz77'\n";
        return 1;
    }
    std::ifstream fin(argv[1], std::ios::binary);

    if (!fin)
    {
        std::cerr << "Error opening input\n"; 
        return 1; 
    }

    std::string data((std::istreambuf_iterator<char>(fin)), {});

    fin.close(); size_t n = data.size();

    std::vector<int> head(HASH_SIZE, -1);
    std::vector<int> next(n, -1);

    struct Match { 
        uint32_t dist; int len; 
    };
    
    std::vector<Match> best(n, {0,0});
    for (size_t i = 0; i + MIN_MATCH <= n; ++i) {
        //  3byte hash
        uint32_t h = ((uint8_t)data[i] << 16) |
                     ((uint8_t)data[i+1] << 8) |
                     ((uint8_t)data[i+2]);
        h ^= h >> 12;
        h &= HASH_SIZE-1;

        int bestLen = 0, bestDist = 0;
        int chain = 0;

        for (int j = head[h]; j >= 0 && chain < MAX_CHAIN; j = next[j], ++chain)
        {
            int dist = int(i) - j;
            if ((size_t)dist > WINDOW_SIZE) continue;
            int k = 0;
            while (i + k < n && data[j+k] == data[i+k]) ++k;
            if (k > bestLen) { bestLen = k; bestDist = dist; }
        }
        best[i] = { uint32_t(bestDist ? bestDist-1:0), bestLen };
        next[i] = head[h]; head[h] = i;
    }

    std::unordered_map<int,uint64_t> lenFreq;
    for (size_t i = 0; i < n; )
    {
        int l = best[i].len;
        if (l >= MIN_MATCH)
        {
             lenFreq[l]++; i += l; 
        }
        else
        {
             i++; 
        }
    }

    if (lenFreq.empty()) lenFreq[MIN_MATCH] = 1;

    std::priority_queue<HNode*,std::vector<HNode*>,HCmp> pq;
    for (auto &p : lenFreq) pq.push(new HNode(p.first, p.second));

    while (pq.size() > 1)
    {
        auto a = pq.top(); pq.pop();
        auto b = pq.top(); pq.pop();
        pq.push(new HNode(a,b));
    }
    HNode* root = pq.top();

    std::unordered_map<int,Code> codeTable;
    buildCodes(root, 0, 0, codeTable);

    std::ofstream fout(argv[2], std::ios::binary);
    uint64_t origSize = n; fout.write((char*)&origSize, sizeof(origSize));
    uint16_t M = lenFreq.size(); fout.write((char*)&M, sizeof(M));
    for (auto &p : lenFreq)
    {
        uint16_t L = p.first; uint64_t F = p.second;
        fout.write((char*)&L, sizeof(L)); fout.write((char*)&F, sizeof(F));
    }

    BitWriter bw(fout);
    for (size_t i = 0; i < n; )
    {
        auto &m = best[i];
        if (m.len >= MIN_MATCH)
        {
            bw.writeBit(1);
            bw.writeBitsUInt(m.dist, DIST_BITS);
            auto &cd = codeTable[m.len];
            bw.writeBitsUInt(cd.bits, cd.len);
            i += m.len;
        } else {
            bw.writeBit(0);
            bw.writeBitsUInt((uint8_t)data[i], 8);
            i++;
        }
    }
    bw.flush(); fout.close(); deleteTree(root);
    return 0;
}
