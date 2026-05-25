/*
 * vector_cfg_test.cc
 *
 * SPEC-3-CFG-001: namespace vector config validation.
 */
#include <gtest/gtest.h>

#include <cstring>

#include "vector/vector_namespace_cfg.h"
#include "vector/vector_types.h"

TEST(VectorCfg, ValidNamespace) // SPEC-3-CFG-001
{
	as_vector_namespace_cfg ns = { 128, AS_VECTOR_VALUE_TYPE_FLOAT,
		AS_VECTOR_METRIC_L2, 512 };

	EXPECT_TRUE(as_vector_namespace_cfg_valid(&ns));
}

TEST(VectorCfg, MissingMandatory) // SPEC-3-CFG-001
{
	as_vector_namespace_cfg ns = { 128, AS_VECTOR_VALUE_TYPE_FLOAT,
		AS_VECTOR_METRIC_BAD, 0 };

	EXPECT_FALSE(as_vector_namespace_cfg_valid(&ns));
}

TEST(VectorCfg, StringParsers) // SPEC-3-CFG-001
{
	as_vector_value_type vt;
	as_vector_metric metric;

	EXPECT_TRUE(as_vector_value_type_from_string("int16", &vt));
	EXPECT_EQ(AS_VECTOR_VALUE_TYPE_INT16, vt);
	EXPECT_TRUE(as_vector_metric_from_string("inner-product", &metric));
	EXPECT_EQ(AS_VECTOR_METRIC_INNER_PRODUCT, metric);
	EXPECT_FALSE(as_vector_value_type_from_string("double", &vt));
	EXPECT_FALSE(as_vector_metric_from_string("dot", &metric));
}

TEST(VectorCfg, QueryBytesLimit) // SPEC-3-CFG-001
{
	as_vector_namespace_cfg ns = { 4, AS_VECTOR_VALUE_TYPE_FLOAT,
		AS_VECTOR_METRIC_L2, 8 };

	EXPECT_FALSE(as_vector_namespace_cfg_valid(&ns));
}

TEST(VectorCfg, RejectsDimensionOverflow) // SPEC-3-CFG-001
{
	// EC528: dim * sizeof(value_type) must fit uint32; values past the cap
	// are rejected even if cfg.c bounds slipped.
	as_vector_namespace_cfg ns = { AS_VECTOR_MAX_DIMENSION + 1,
		AS_VECTOR_VALUE_TYPE_FLOAT, AS_VECTOR_METRIC_L2, 0 };

	EXPECT_FALSE(as_vector_namespace_cfg_valid(&ns));
}

TEST(VectorCfg, AcceptsMaxDimension) // SPEC-3-CFG-001
{
	as_vector_namespace_cfg ns = { AS_VECTOR_MAX_DIMENSION,
		AS_VECTOR_VALUE_TYPE_FLOAT, AS_VECTOR_METRIC_L2, 0 };

	EXPECT_TRUE(as_vector_namespace_cfg_valid(&ns));
}
