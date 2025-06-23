#include "core/HashRing.hpp"
#include <gtest/gtest.h>

TEST(HashRing, Basic) {
    HashRing ring;
    ring.addNode("A", 10);
    ring.addNode("B", 10);
    EXPECT_EQ(ring.size(), 20);
    std::string k = "hello";
    auto n = ring.getNode(k);
    EXPECT_TRUE(n == "A" || n == "B");
}
