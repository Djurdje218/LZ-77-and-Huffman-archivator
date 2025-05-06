#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <cstdint>

constexpr std::size_t WINDOW_SIZE = 32768;
constexpr int DIST_BITS = 15;

class BitReader {
    std::ifstream& in_;
    uint8_t buf_ = 0;
    uint8_t left_ = 0;
public:
    explicit BitReader(std::ifstream& in) : in_(in) {}
    int bit() {
        if (!left_) {
            int ch = in_.get();
            if (ch == EOF) return -1;
            buf_ = static_cast<uint8_t>(ch);
            left_ = 8;
        }
        int b = (buf_ >> 7) & 1;
        buf_ <<= 1;
        --left_;
        return b;
    }
};

struct Node {
    int   sym;
    Node* l;
    Node* r;
    explicit Node(int s) : sym(s), l(nullptr), r(nullptr) {}
    Node(Node* L, Node* R) : sym(-1), l(L), r(R) {}
};

static Node* buildHuffmanTree(const std::vector<std::pair<int, uint64_t>>& t)
{
    struct NF  { Node* n; uint64_t f; };
    struct Cmp { bool operator()(const NF& a, const NF& b) const { return a.f > b.f; } };

    std::priority_queue<NF, std::vector<NF>, Cmp> pq;

    for (auto [len, f] : t)
        pq.push({ new Node(len), f });

    if (pq.empty())
        pq.push({ new Node(1), 1 });

    while (pq.size() > 1)
    {
        auto a = pq.top(); pq.pop();
        auto b = pq.top(); pq.pop();

        pq.push({ new Node(a.n, b.n), a.f + b.f });
    }
    return pq.top().n;
}


int main(int argc, char** argv) {
    if (argc != 3) { std::cerr << "Usage: decoder <in> <out>\n"; return 1; }

    std::ifstream fin(argv[1], std::ios::binary);
    if (!fin) { std::cerr << "cannot open input\n"; return 1; }

    uint64_t orig; fin.read(reinterpret_cast<char*>(&orig), sizeof(orig));
    uint16_t sz;   fin.read(reinterpret_cast<char*>(&sz),   sizeof(sz));

    std::vector<std::pair<int, uint64_t>> table;
    table.reserve(sz);
    for (int i = 0; i < sz; ++i) {
        uint16_t L; uint64_t F;
        fin.read(reinterpret_cast<char*>(&L), sizeof(L));
        fin.read(reinterpret_cast<char*>(&F), sizeof(F));
        table.emplace_back(int(L), F);
    }

    Node* root = buildHuffmanTree(table);
    BitReader br(fin);

    std::vector<char> out;
    out.reserve(orig);

    while (out.size() < orig) {
        int flag = br.bit();
        if (flag < 0) break;

        if (!flag) {
            unsigned char c = 0;
            for (int i = 0; i < 8; ++i) c = (c << 1) | br.bit();
            out.push_back(char(c));
        } else {
            uint32_t d = 0;
            for (int i = 0; i < DIST_BITS; ++i) d = (d << 1) | br.bit();
            std::size_t dist = d + 1;

            Node* n = root;
            while (n->l || n->r) n = br.bit() ? n->r : n->l;
            int len = n->sym;

            std::size_t base = out.size() - dist;
            for (int k = 0; k < len; ++k) out.push_back(out[base + k]);
        }
    }

    std::ofstream fout(argv[2], std::ios::binary);
    fout.write(out.data(), out.size());
    return 0;
}
