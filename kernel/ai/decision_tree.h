// NeuronOS Decision Tree Inference Engine
// kernel/ai/decision_tree.h

#ifndef DECISION_TREE_H
#define DECISION_TREE_H

#include <stdint.h>

// A node in the decision tree
typedef struct {
    int feature_index;       // Which feature to evaluate (-1 for leaf node)
    float threshold;         // Threshold for the evaluation
    int left_child;          // Node index if feature <= threshold
    int right_child;         // Node index if feature > threshold
    float leaf_value;        // Output value if this is a leaf node
} dt_node_t;

// A decision tree model
typedef struct {
    dt_node_t* nodes;
    int num_nodes;
    int num_features;
} dt_model_t;

// Initialize the inference engine
void dt_init(void);

// Load a model from userspace/disk into the kernel
int dt_load_model(dt_model_t* model, const dt_node_t* nodes, int num_nodes, int num_features);

// Evaluate a decision tree
float dt_predict(const dt_model_t* model, const float* features);

#endif // DECISION_TREE_H
