// Copyright (c) Facebook, Inc. and its affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the "hack" directory of this source tree.
//
// @generated SignedSource<<11068eda76ab827a36a93ce35756e1d6>>
//
// To regenerate this file, run:
//   hphp/hack/src/oxidized_regen.sh

use no_pos_hash::NoPosHash;
use ocamlrep_derive::FromOcamlRep;
use ocamlrep_derive::ToOcamlRep;
use serde::Deserialize;
use serde::Serialize;

#[allow(unused_imports)]
use crate::*;

pub use crate::ast_defs::shape_map;

pub use aast::ClassHint;
pub use aast::Hint;
pub use aast::Sid;
pub use aast::TraitHint;
pub use aast::XhpAttrHint;
pub use ast_defs::ShapeFieldName;

pub type Program = aast::Program<(), ()>;

pub type Def = aast::Def<(), ()>;

pub type Expr = aast::Expr<(), ()>;

pub type Expr_ = aast::Expr_<(), ()>;

pub type Stmt = aast::Stmt<(), ()>;

pub type Block = aast::Block<(), ()>;

pub type UserAttribute = aast::UserAttribute<(), ()>;

pub type ClassId_ = aast::ClassId_<(), ()>;

pub type Class_ = aast::Class_<(), ()>;

pub type ClassVar = aast::ClassVar<(), ()>;

pub type Method_ = aast::Method_<(), ()>;

pub type FileAttribute = aast::FileAttribute<(), ()>;

pub type Fun_ = aast::Fun_<(), ()>;

pub type FunDef = aast::FunDef<(), ()>;

pub type FuncBody = aast::FuncBody<(), ()>;

pub type FunParam = aast::FunParam<(), ()>;

pub type Typedef = aast::Typedef<(), ()>;

pub type Tparam = aast::Tparam<(), ()>;

pub type Gconst = aast::Gconst<(), ()>;

pub type ClassConst = aast::ClassConst<(), ()>;

pub type ClassId = aast::ClassId<(), ()>;

pub type Catch = aast::Catch<(), ()>;

pub type Case = aast::Case<(), ()>;

pub type Field = aast::Field<(), ()>;

pub type Afield = aast::Afield<(), ()>;

pub type XhpAttribute = aast::XhpAttribute<(), ()>;

pub type ExpressionTree = aast::ExpressionTree<(), ()>;

pub type Targ = aast::Targ<()>;

pub type TypeHint = aast::TypeHint<()>;

#[derive(
    Clone,
    Debug,
    Deserialize,
    Eq,
    FromOcamlRep,
    Hash,
    NoPosHash,
    Ord,
    PartialEq,
    PartialOrd,
    Serialize,
    ToOcamlRep
)]
#[repr(C)]
pub struct IgnoreAttributeEnv {
    pub ignored_attributes: Vec<String>,
}
