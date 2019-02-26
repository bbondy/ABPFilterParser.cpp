/* Copyright (c) 2019 The Brave Software Team. Distributed under the MPL2
 * license. This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <iostream>
#include "etld/internal/public_suffix_rule.h"
#include "etld/internal/public_suffix_rule_set.h"
#include "etld/domain.h"
#include "etld/types.h"
#include "etld/serialization.h"

using brave_etld::internal::PublicSuffixRuleSetMatchResult;

namespace brave_etld {
namespace internal {

PublicSuffixRuleSet::PublicSuffixRuleSet() {}

PublicSuffixRuleSet::PublicSuffixRuleSet(const PublicSuffixRuleSet& rule_set) {
  for (auto &elm : rule_set.rules_) {
    AddRule(*elm);
  }
}

PublicSuffixRuleSet::PublicSuffixRuleSet(
    const std::vector<PublicSuffixRule>& rules) {
  for (auto &elm : rules) {
    AddRule(elm);
  }
}

PublicSuffixRuleSet::~PublicSuffixRuleSet() {
  delete root_;
}

bool PublicSuffixRuleSet::Equal(const PublicSuffixRuleSet& rule_set) const {
  if (rules_.size() != rule_set.rules_.size()) {
    return false;
  }

  bool found_match;
  for (auto &local_elm : rules_) {
    found_match = false;
    for (auto &other_elm : rule_set.rules_) {
     if (local_elm->Equals(*other_elm)) {
       found_match = true;
       break;
     }
    }
    if (!found_match) {
      return false;
    }
  }

  return true;
}

SerializationResult PublicSuffixRuleSet::Serialize() const {
  std::stringstream buffer_stream;
  for (auto &elm : rules_) {
    buffer_stream << elm->Serialize().buffer;
  }

  const std::string buffer_body = buffer_stream.str();
  const size_t body_len = buffer_body.size();
  const std::string header_str = std::to_string(body_len);
  const size_t header_len = header_str.size();
  const size_t body_start = header_len + 1;
  const size_t buffer_size = body_start + body_len + 1;
  char* buffer = reinterpret_cast<char*>(malloc(sizeof(char) * buffer_size));

  snprintf(
    buffer,
    buffer_size,
    "%s:%s",
    header_str.c_str(),
    buffer_body.c_str());

  const std::string serialized_buffer(buffer);
  free(buffer);

  return {
    serialized_buffer,
    body_start,
    body_len
  };
}

PublicSuffixRuleSetMatchResult PublicSuffixRuleSet::Match(
    const Domain& domain) const {
  if (root_ == nullptr) {
    return {false, nullptr};
  }

  const size_t domain_length = domain.Length();
  if (domain_length == 0) {
    return {false, nullptr};
  }

  const PublicSuffixRule* match = nullptr;
  MatchRecursions(domain, domain_length - 1, &match, root_);

  if (match == nullptr) {
    return {false, nullptr};
  }

  return {true, match};
}

void PublicSuffixRuleSet::MatchRecursions(const Domain& domain,
    const size_t label_index, const PublicSuffixRule** match,
    PublicSuffixRuleMapNode* node) const {
  const Label& current_label = domain.Get(label_index);

  const auto label_result = node->children->find(current_label);
  if (label_result != node->children->end()) {
    PublicSuffixRuleMapNode* child_node = node->children->at(current_label);
    if (child_node->rule != nullptr) {
      if (*match == nullptr ||
          child_node->rule->Length() > (*match)->Length()) {
        *match = child_node->rule;
      }
    }
    if (label_index > 0) {
      MatchRecursions(domain, label_index - 1, match, child_node);
    }
  }

  const auto wildcard_result = node->children->find("*");
  if (wildcard_result != node->children->end()) {
    PublicSuffixRuleMapNode* child_node = node->children->at("*");
    if (child_node->rule != nullptr) {
      if (*match == nullptr ||
          child_node->rule->Length() > (*match)->Length()) {
        *match = child_node->rule;
      }
    }
    if (label_index > 0) {
      MatchRecursions(domain, label_index - 1, match, child_node);
    }
  }
}

void PublicSuffixRuleSet::AddRule(const PublicSuffixRule& rule) {
  const int num_rules = static_cast<int>(rule.Length());
  if (root_ == nullptr) {
    root_ = new PublicSuffixRuleMapNode();
  }
  const size_t last_label_index = num_rules - 1;
  AddRule(rule, last_label_index, root_);
}

void PublicSuffixRuleSet::AddRule(const PublicSuffixRule& rule,
    const size_t label_index, PublicSuffixRuleMapNode* node) {
  const Label& current_label = rule.Get(label_index);
  const auto find_result = node->children->find(current_label);
  PublicSuffixRuleMapNode *child_node;
  if (find_result == node->children->end()) {
    child_node = new PublicSuffixRuleMapNode();
    node->children->emplace(current_label, child_node);
  } else {
    child_node = node->children->at(current_label);
  }

  if (label_index == 0) {
    PublicSuffixRule* new_rule = new PublicSuffixRule(rule);
    child_node->rule = new_rule;
    rules_.push_back(new_rule);
    return;
  }

  AddRule(rule, label_index - 1, child_node);
}

PublicSuffixRuleSet rule_set_from_serialization(
    const SerializedBuffer& buffer) {
  std::vector<PublicSuffixRule> rules;
  for (auto &elm : deserialize_buffer(buffer)) {
    rules.push_back(rule_from_serialization(elm));
  }
  return PublicSuffixRuleSet(rules);
}

}  // namespace internal
}  // namespace brave_etld
