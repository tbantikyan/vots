#pragma once

#include <memory>

namespace vots {

#define RBTREE_TEMPLATE template <typename KeyType, typename DataType>
#define RBTREE_TYPE RedBlackTree<KeyType, DataType>

/*
 * RedBlackTree implements a Red-Black tree and is used to store limit objects.
 *
 * The self-balancing property of Red-Black trees ensures that the bid and ask limit trees always provide worst-case
 * O(log m) operations, where m is the size of the tree. A Red-Black tree is preferred over other self-balancing
 * trees as its relaxed balancing property result in fewer (costly) rotations in the write-heavy workloads typical
 * of the market. This comes at the trade-off of less-efficient lookups, but the use of a hashmap for tracking limit
 * prices eliminates the need for lookups in the limit trees altogether.
 */
RBTREE_TEMPLATE class RedBlackTree {
   public:
    class Node {
        friend class RBTREE_TYPE;

       private:
        DataType data_;
        Node *parent_;
        std::unique_ptr<Node> left_;
        std::unique_ptr<Node> right_;
        KeyType key_;
        bool is_red_;
        bool is_nil_;  // for leave's children

        Node(DataType data, Node *parent, KeyType key, bool is_red, bool is_nil)
            : data_(std::move(data)),
              parent_(parent),
              left_(nullptr),
              right_(nullptr),
              key_(key),
              is_red_(is_red),
              is_nil_(is_nil) {}
    };

    RedBlackTree();

    // At returns a nullptr if the data with the specified key was not found
    auto At(KeyType key) -> DataType *;

    // Return the minimum value in the tree. If the tree is empty, returns nullptr
    auto GetMin() -> Node * {
        if (min_node_ == nullptr) {
            return nullptr;
        }
        return min_node_;
    }

    // Return the maximum value in the tree. If the tree is empty, returns nullptr
    auto GetMax() -> Node * {
        if (max_node_ == nullptr) {
            return nullptr;
        }
        return max_node_;
    }

    auto Insert(KeyType key, DataType data) -> Node &;
    void Delete(KeyType key);
    void Delete(Node &node);

    auto ValidateTree(int &count) -> bool;

   private:
    std::unique_ptr<Node> root_;
    Node *max_node_ = nullptr;
    Node *min_node_ = nullptr;

    // InsertFix restores violated tree invariants, if any are present, after an insert
    void InsertFix(std::unique_ptr<Node> *node);
    // DeleteFix restores violated tree invariants, if any are present, after a delete
    void DeleteFix(std::unique_ptr<Node> *Node);

    void RotateLeft(std::unique_ptr<Node> &node);
    void RotateRight(std::unique_ptr<Node> &node);

    // NewNode returns a red node with the specified key, data, and parent
    auto NewNode(KeyType key, DataType data, Node *parent) -> std::unique_ptr<Node> {
        auto n = std::unique_ptr<Node>(new Node(data, parent, key, true, false));
        n->left_ = std::move(NewDummyNil(n.get()));
        n->right_ = std::move(NewDummyNil(n.get()));
        return n;
    }
    // NewDummyNil returns a node representing a parent's NIL child (by spec., these are black)
    auto NewDummyNil(Node *parent) -> std::unique_ptr<Node> {
        return std::unique_ptr<Node>(new Node(DataType{}, parent, KeyType{}, false, true));
    }

    // GetNodeOwner returns the unique_ptr owning the provided, not_null Node pointer
    auto GetNodeOwner(Node *node) -> std::unique_ptr<Node> & {
        Node *parent = node->parent_;
        if (parent == nullptr) {
            return this->root_;
        }

        if (parent->left_.get() == node) {
            return parent->left_;
        }
        return parent->right_;
    }

    // Finds new minimum node discounting tree's current minimum
    auto FindNewMin() -> Node * {
        Node *right_child = this->min_node_->right_.get();
        if (!right_child->is_nil_) {
            return right_child;
        }

        return this->min_node_->parent_;
    }

    // Finds new maximum node discounting tree's current maximum
    auto FindNewMax() -> Node * {
        Node *left_child = this->max_node_->left_.get();
        if (!left_child->is_nil_) {
            return left_child;
        }

        return this->max_node_->parent_;
    }

    auto FindDeleteReplacement(Node *to_delete) -> std::unique_ptr<Node> &;
    void ReplaceDeleted(Node *to_delete, std::unique_ptr<Node> *replacement);
};

}  // namespace vots
