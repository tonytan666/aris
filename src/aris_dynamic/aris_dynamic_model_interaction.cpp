﻿#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <limits>
#include <sstream>
#include <regex>
#include <limits>
#include <type_traits>

#include "aris_dynamic_model.h"

namespace aris
{
	namespace dynamic
	{
		auto Interaction::saveXml(aris::core::XmlElement &xml_ele) const->void
		{
			DynEle::saveXml(xml_ele);

			xml_ele.SetAttribute("prt_m", this->makI().fatherPart().name().c_str());
			xml_ele.SetAttribute("prt_n", this->makJ().fatherPart().name().c_str());
			xml_ele.SetAttribute("mak_i", this->makI().name().c_str());
			xml_ele.SetAttribute("mak_j", this->makJ().name().c_str());
		}
		auto Interaction::loadXml(const aris::core::XmlElement &xml_ele)->void
		{
			if (model().findByName("part_pool") == model().children().end())
				throw std::runtime_error("you must insert \"part_pool\" node before insert " + type() + " \"" + name() + "\"");
			auto &part_pool = static_cast<aris::core::ObjectPool<Part, Element>&>(*model().findByName("part_pool"));

			if (!xml_ele.Attribute("prt_m"))throw std::runtime_error(std::string("xml element \"") + name() + "\" must have Attribute \"prt_m\"");
			auto prt_m = part_pool.findByName(xml_ele.Attribute("prt_m"));
			if (prt_m == part_pool.end())	throw std::runtime_error(std::string("can't find part m for element \"") + this->name() + "\"");

			if (!xml_ele.Attribute("mak_i"))throw std::runtime_error(std::string("xml element \"") + name() + "\" must have Attribute \"mak_i\"");
			auto mak_i = prt_m->markerPool().findByName(xml_ele.Attribute("mak_i"));
			if (mak_i == prt_m->markerPool().end())
				throw std::runtime_error(std::string("can't find marker i for element \"") + this->name() + "\"");
			makI_ = &(*mak_i);

			if (!xml_ele.Attribute("prt_n"))throw std::runtime_error(std::string("xml element \"") + name() + "\" must have Attribute \"prt_n\"");
			auto prt_n = part_pool.findByName(xml_ele.Attribute("prt_n"));
			if (prt_n == part_pool.end())throw std::runtime_error(std::string("can't find part n for element \"") + this->name() + "\"");

			if (!xml_ele.Attribute("mak_j"))throw std::runtime_error(std::string("xml element \"") + name() + "\" must have Attribute \"mak_j\"");
			auto mak_j = prt_n->markerPool().findByName(xml_ele.Attribute("mak_j"));
			if (mak_j == prt_n->markerPool().end())throw std::runtime_error(std::string("can't find marker j for element \"") + this->name() + "\"");
			makJ_ = &(*mak_j);

			DynEle::loadXml(xml_ele);
		}

		struct Constraint::Imp { Size col_id_, blk_col_id_; double cf_[6]{ 0 }; };
		auto Constraint::saveXml(aris::core::XmlElement &xml_ele) const->void
		{
			Interaction::saveXml(xml_ele);
			xml_ele.SetAttribute("cf", core::Matrix(1, dim(), cf()).toString().c_str());
		}
		auto Constraint::loadXml(const aris::core::XmlElement &xml_ele)->void
		{
			Interaction::loadXml(xml_ele);
			setCf(attributeMatrix(xml_ele, "cf", 1, dim(), core::Matrix(1, dim(), 0.0)).data());
		}
		auto Constraint::cf() const->const double* { return imp_->cf_; }
		auto Constraint::setCf(const double *cf)->void { return s_vc(dim(), cf, imp_->cf_); }
		auto Constraint::cptCpFromPm(double *cp, const double *makI_pm, const double *makJ_pm)const->void
		{
			double pm_j2i[16], ps_j2i[6];
			s_inv_pm_dot_pm(makI_pm, makJ_pm, pm_j2i);
			s_pm2ps(pm_j2i, ps_j2i);
			s_mm(dim(), 1, 6, locCmI(), ColMajor{ dim() }, ps_j2i, 1, cp, 1);
		}
		auto Constraint::cptCv(double *cv)const->void
		{
			double dv[6], dv_in_I[6];
			s_vc(6, makJ().vs(), dv);
			s_vs(6, makI().vs(), dv);
			s_inv_tv(*makI().pm(), dv, dv_in_I);
			s_mm(dim(), 1, 6, locCmI(), ColMajor{ dim() }, dv_in_I, 1, cv, 1);
		};
		auto Constraint::cptCa(double *ca)const->void
		{
			double vi_cross_vj[6], tem[6];
			s_cv(makI().vs(), makJ().vs(), vi_cross_vj);
			s_inv_tv(*makI().pm(), vi_cross_vj, tem);
			s_mmi(dim(), 1, 6, locCmI(), ColMajor{ dim() }, tem, 1, ca, 1);
		}
		Constraint::~Constraint() = default;
		Constraint::Constraint(const std::string &name, Marker* makI, Marker* makJ, bool is_active) : Interaction(name, makI, makJ, is_active) {}
		Constraint::Constraint(const Constraint&) = default;
		Constraint::Constraint(Constraint&&) = default;
		Constraint& Constraint::operator=(const Constraint&) = default;
		Constraint& Constraint::operator=(Constraint&&) = default;

		Joint::Joint(Joint &&other) = default;
		Joint::Joint(const Joint &other) = default;
		Joint& Joint::operator=(const Joint &other) = default;
		Joint& Joint::operator=(Joint &&other) = default;

		struct Motion::Imp
		{
			Size clb_frc_id_{ 0 }, clb_id_{ 0 };
			Size component_axis_;
			double frc_coe_[3]{ 0,0,0 };
			double mp_offset_{ 0 }, mp_factor_{ 1.0 };
			double mp_{ 0 }, mv_{ 0 }, ma_{ 0 };

			double loc_cm_I[6];
		};
		auto Motion::saveXml(aris::core::XmlElement &xml_ele) const->void
		{
			Constraint::saveXml(xml_ele);

			xml_ele.SetAttribute("frc_coe", core::Matrix(1, 3, this->frcCoe()).toString().c_str());
			xml_ele.SetAttribute("component", static_cast<int>(axis()));
			xml_ele.SetAttribute("mp", mp());
			xml_ele.SetAttribute("mv", mv());
			xml_ele.SetAttribute("ma", ma());
			if (imp_->mp_offset_ != 0)xml_ele.SetAttribute("mp_offset", imp_->mp_offset_);
			if (imp_->mp_factor_ != 1.0)xml_ele.SetAttribute("mp_factor", imp_->mp_factor_);
		}
		auto Motion::loadXml(const aris::core::XmlElement &xml_ele)->void
		{
			imp_->component_axis_ = attributeInt32(xml_ele, "component");
			imp_->mp_offset_ = attributeDouble(xml_ele, "mp_offset", 0.0);
			imp_->mp_factor_ = attributeDouble(xml_ele, "mp_factor", 1.0);
			imp_->mp_ = attributeDouble(xml_ele, "mp", 0.0);
			imp_->mv_ = attributeDouble(xml_ele, "mv", 0.0);
			imp_->ma_ = attributeDouble(xml_ele, "ma", 0.0);

			setFrcCoe(attributeMatrix(xml_ele, "frc_coe", 1, 3).data());

			Constraint::loadXml(xml_ele);
			s_fill(1, 6, 0.0, const_cast<double*>(locCmI()));
			const_cast<double*>(locCmI())[axis()] = 1.0;
		}
		auto Motion::locCmI() const->const double* { return imp_->loc_cm_I;	}
		auto Motion::cptCpFromPm(double *cp, const double *makI_pm, const double *makJ_pm)const->void
		{
			Constraint::cptCpFromPm(cp, makI_pm, makJ_pm);
			cp[0] += mp();
		}
		auto Motion::cptCv(double *cv)const->void { Constraint::cptCv(cv); cv[0] += mv(); }
		auto Motion::cptCa(double *ca)const->void { Constraint::cptCa(ca); ca[0] += ma(); }
		auto Motion::updMp()->void { setMp(s_sov_axis_distance(*makJ().pm(), *makI().pm(), axis())); }
		auto Motion::updMv()->void
		{
			double vs_i2j[6];
			makI().getVs(makJ(), vs_i2j);
			setMv(vs_i2j[axis()]);
		}
		auto Motion::updMa()->void
		{
			double as_i2j[6];
			makI().getAs(makJ(), as_i2j);
			setMa(as_i2j[axis()]);
		}
		auto Motion::axis()const->Size { return imp_->component_axis_; }
		auto Motion::frcCoe()const->const double3&{ return imp_->frc_coe_; }
		auto Motion::setFrcCoe(const double *frc_coe)->void { std::copy_n(frc_coe, 3, imp_->frc_coe_); }
		auto Motion::mp() const->double { return imp_->mp_ / imp_->mp_factor_ - imp_->mp_offset_; }
		auto Motion::setMp(double mp)->void { imp_->mp_ = (mp + imp_->mp_offset_) * imp_->mp_factor_; }
		auto Motion::mv() const->double { return imp_->mv_; }
		auto Motion::setMv(double mv)->void { imp_->mv_ = mv; }
		auto Motion::ma() const->double { return imp_->ma_; }
		auto Motion::setMa(double ma)->void { imp_->ma_ = ma; }
		auto Motion::mf() const->double { return mfDyn() + mfFrc(); }
		auto Motion::setMf(double mf)->void { Constraint::imp_->cf_[0] = mf - mfFrc(); }
		auto Motion::mfDyn() const->double { return Constraint::imp_->cf_[0]; }
		auto Motion::setMfDyn(double mf_dyn)->void { Constraint::imp_->cf_[0] = mf_dyn; }
		auto Motion::mfFrc() const->double { return s_sgn(imp_->mv_)*frcCoe()[0] + imp_->mv_*frcCoe()[1] + imp_->ma_*frcCoe()[2]; }
		Motion::~Motion() = default;
		Motion::Motion(const std::string &name, Marker* makI, Marker* makJ, Size component_axis, const double *frc_coe, double mp_offset, double mp_factor, bool active) : Constraint(name, makI, makJ, active)
		{
			imp_->component_axis_ = component_axis;
			imp_->mp_offset_ = mp_offset;
			imp_->mp_factor_ = mp_factor;
			
			static const double default_frc_coe[3]{ 0,0,0 };
			setFrcCoe(frc_coe ? frc_coe : default_frc_coe);

			s_fill(1, 6, 0.0, const_cast<double*>(locCmI()));
			const_cast<double*>(locCmI())[axis()] = 1.0;
		}
		Motion::Motion(const Motion &other) = default;
		Motion::Motion(Motion &&other) = default;
		Motion& Motion::operator=(const Motion &other) = default;
		Motion& Motion::operator=(Motion &&other) = default;

		struct GeneralMotion::Imp
		{
			double mpm_[4][4]{ { 0 } }, mvs_[6]{ 0 }, mas_[6]{ 0 };
		};
		auto GeneralMotion::locCmI() const->const double* 
		{
			static const double loc_cm_I[36]{ 1,0,0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,1 };
			return loc_cm_I;
		}
		auto GeneralMotion::cptCpFromPm(double *cp, const double *makI_pm, const double *makJ_pm)const->void
		{
			// Pi : mak I 的实际位置
			// Pj : mak J 的实际位置
			// Pit: mak I 应该达到的位置
			// Pc : 需补偿的位姿
			// 理论上应该有：
			// Pi = Pj * mpm
			// 那么就有：
			// Pit = Pj * mpm
			// 于是：
			// Pc = Pi^-1 * Pit

			double pm_it[16];
			s_pm_dot_pm(makJ_pm, *mpm(), pm_it);

			double pm_c[16], ps_c[6];
			s_inv_pm_dot_pm(makI_pm, pm_it, pm_c);
			s_pm2ps(pm_c, ps_c);

			// locCmI为单位矩阵，此时无需相乘
			s_vc(6, ps_c, cp);
		}
		auto GeneralMotion::cptCv(double *cv)const->void { Constraint::cptCv(cv); s_inv_tva(*mpm(), mvs(), cv); }
		auto GeneralMotion::cptCa(double *ca)const->void { s_inv_tv(*mpm(), mas(), ca); }
		auto GeneralMotion::mpm()const->const double4x4&{ return imp_->mpm_; }
		auto GeneralMotion::updMpm()->void { s_inv_pm_dot_pm(*makJ().pm(), *makI().pm(), *imp_->mpm_); }
		auto GeneralMotion::setMpe(const double* pe, const char *type)->void { s_pe2pm(pe, *imp_->mpm_, type); }
		auto GeneralMotion::setMpq(const double* pq)->void { s_pq2pm(pq, *imp_->mpm_); }
		auto GeneralMotion::setMpm(const double* pm)->void { s_vc(16, pm, *imp_->mpm_); }
		auto GeneralMotion::getMpe(double* pe, const char *type)const->void { s_pm2pe(*imp_->mpm_, pe, type); }
		auto GeneralMotion::getMpq(double* pq)const->void { s_pm2pq(*imp_->mpm_, pq); }
		auto GeneralMotion::getMpm(double* pm)const->void { s_vc(16, *imp_->mpm_, pm); }
		auto GeneralMotion::mvs()const->const double6&{ return imp_->mvs_; }
		auto GeneralMotion::updMvs()->void { s_inv_vs2vs(*makJ().pm(), makJ().vs(), makI().vs(), imp_->mvs_); }
		auto GeneralMotion::setMve(const double* ve, const char *type)->void
		{
			double pe[6];
			s_pm2pe(*mpm(), pe, type);
			s_ve2vs(pe, ve, imp_->mvs_, type);
		}
		auto GeneralMotion::setMvq(const double* vq)->void
		{
			double pq[7];
			s_pm2pq(*mpm(), pq);
			s_vq2vs(pq, vq, imp_->mvs_);
		}
		auto GeneralMotion::setMvm(const double* vm)->void { s_vm2vs(*mpm(), vm, imp_->mvs_); }
		auto GeneralMotion::setMva(const double* va)->void
		{
			double pp[3];
			s_pm2pp(*mpm(), pp);
			s_va2vs(pp, va, imp_->mvs_);
		}
		auto GeneralMotion::setMvs(const double* vs)->void { s_vc(6, vs, imp_->mvs_); }
		auto GeneralMotion::getMve(double* ve, const char *type)const->void
		{
			double pe[6];
			s_pm2pe(*mpm(), pe, type);
			s_vs2ve(imp_->mvs_, pe, ve, type);
		}
		auto GeneralMotion::getMvq(double* vq)const->void
		{
			double pq[7];
			s_pm2pq(*mpm(), pq);
			s_vs2vq(imp_->mvs_, pq, vq);
		}
		auto GeneralMotion::getMvm(double* vm)const->void { s_vs2vm(imp_->mvs_, *mpm(), vm); }
		auto GeneralMotion::getMva(double* va)const->void
		{
			double pp[3];
			s_pm2pp(*mpm(), pp);
			s_vs2va(imp_->mvs_, pp, va);
		}
		auto GeneralMotion::getMvs(double* vs)const->void { s_vc(6, imp_->mvs_, vs); }
		auto GeneralMotion::mas()const->const double6&{ return imp_->mas_; }
		auto GeneralMotion::updMas()->void { s_inv_as2as(*makJ().pm(), makJ().vs(), makJ().as(), makI().vs(), makI().as(), imp_->mas_); }
		auto GeneralMotion::setMae(const double* ae, const char *type)->void
		{
			double pe[6], ve[6];
			s_pm2pe(*mpm(), pe, type);
			s_vs2ve(mvs(), pe, ve, type);
			s_ae2as(pe, ve, ae, imp_->mas_, nullptr, type);
		}
		auto GeneralMotion::setMaq(const double* aq)->void
		{
			double pq[7], vq[7];
			s_pm2pq(*mpm(), pq);
			s_vs2vq(mvs(), pq, vq);
			s_aq2as(pq, vq, aq, imp_->mas_);
		}
		auto GeneralMotion::setMam(const double* am)->void
		{
			double vm[16];
			getMvm(vm);
			s_am2as(*mpm(), vm, am, imp_->mas_);
		}
		auto GeneralMotion::setMaa(const double* aa)->void
		{
			double pp[3], va[6];
			s_pm2pp(*mpm(), pp);
			s_vs2va(mvs(), pp, va);
			s_aa2as(pp, va, aa, imp_->mas_);
		}
		auto GeneralMotion::setMas(const double* as)->void { s_vc(6, as, imp_->mas_); }
		auto GeneralMotion::getMae(double* ae, const char *type)const->void
		{
			double pe[6];
			s_pm2pe(*mpm(), pe, type);
			s_as2ae(mvs(), mas(), pe, ae, nullptr, type);
		}
		auto GeneralMotion::getMaq(double* aq)const->void
		{
			double pq[7];
			s_pm2pq(*mpm(), pq);
			s_as2aq(mvs(), mas(), pq, aq);
		}
		auto GeneralMotion::getMam(double* am)const->void { s_as2am(mvs(), mas(), *mpm(), am); }
		auto GeneralMotion::getMaa(double* aa)const->void
		{
			double pp[3];
			s_pm2pp(*mpm(), pp);
			s_as2aa(mvs(), mas(), pp, aa);
		}
		auto GeneralMotion::getMas(double* as)const->void { s_vc(6, imp_->mas_, as); }
		auto GeneralMotion::mfs() const->const double6&{ return Constraint::imp_->cf_; }
		auto GeneralMotion::setMfs(const double * mfs)->void { s_vc(6, mfs, Constraint::imp_->cf_); }
		GeneralMotion::~GeneralMotion() = default;
		GeneralMotion::GeneralMotion(const std::string &name, Marker* makI, Marker* makJ, bool active) :Constraint(name, makI, makJ, active){}
		GeneralMotion::GeneralMotion(const GeneralMotion &other) = default;
		GeneralMotion::GeneralMotion(GeneralMotion &&other) = default;
		GeneralMotion& GeneralMotion::operator=(const GeneralMotion &other) = default;
		GeneralMotion& GeneralMotion::operator=(GeneralMotion &&other) = default;

		auto RevoluteJoint::locCmI() const->const double*
		{
			static const double loc_cm_I[30]
			{   
				1,0,0,0,0,
				0,1,0,0,0,
				0,0,1,0,0,
				0,0,0,1,0,
				0,0,0,0,1,
				0,0,0,0,0  
			};
			return loc_cm_I;
		}
		auto RevoluteJoint::cptCpFromPm(double *cp, const double *makI_pm, const double *makJ_pm)const->void
		{
			double pm_j_in_i[16];
			s_inv_pm_dot_pm(makI_pm, makJ_pm, pm_j_in_i);

			cp[0] = pm_j_in_i[3];
			cp[1] = pm_j_in_i[7];
			cp[2] = pm_j_in_i[11];

			// 这里用i的z轴叉乘j的z轴，在i坐标系下，因此叉乘出来有如下结果:
			cp[3] = -pm_j_in_i[6];
			cp[4] = pm_j_in_i[2];
		}
		RevoluteJoint::RevoluteJoint(const std::string &name, Marker* makI, Marker* makJ): Joint(name, makI, makJ){}
		
		auto PrismaticJoint::locCmI() const->const double*
		{
			static const double loc_cm_I[30]
			{
				1,0,0,0,0,
				0,1,0,0,0,
				0,0,0,0,0,
				0,0,1,0,0,
				0,0,0,1,0,
				0,0,0,0,1
			};
			return loc_cm_I;
		}
		auto PrismaticJoint::cptCpFromPm(double *cp, const double *makI_pm, const double *makJ_pm)const->void
		{
			double pm_j2i[16], ps_j2i[6];
			s_inv_pm_dot_pm(makI_pm, makJ_pm, pm_j2i);
			s_pm2ps(pm_j2i, ps_j2i);

			// 此时位移差值在makI()坐标系中
			s_vc(2, ps_j2i, cp);
			s_vc(3, ps_j2i + 3, cp + 2);
		}
		PrismaticJoint::PrismaticJoint(const std::string &name, Marker* makI, Marker* makJ): Joint(name, makI, makJ){}

		struct UniversalJoint::Imp{	double loc_cm_I[24];};
		auto UniversalJoint::locCmI() const->const double*
		{
			const double axis_iz_i[3]{ 0,0,1 };
			double axis_jz_g[3], axis_jz_m[3];

			s_pm_dot_v3(*makJ().fatherPart().pm(), &makJ().prtPm()[0][2], 4, axis_jz_g, 1);
			s_inv_pm_dot_v3(*makI().fatherPart().pm(), axis_jz_g, axis_jz_m);

			// 应该求 axis_iz_i(x1 y1 z1) x axis_jz_i(x2 y2 z2), 但是因为axis_iz_i为单位向量(0,0,1)
			// 那么，这里应该为：
			// [ -y2 x2 0 ]
			double x2 = makI().prtPm()[0][0] * axis_jz_m[0] + makI().prtPm()[1][0] * axis_jz_m[1] + makI().prtPm()[2][0] * axis_jz_m[2];
			double y2 = makI().prtPm()[0][1] * axis_jz_m[0] + makI().prtPm()[1][1] * axis_jz_m[1] + makI().prtPm()[2][1] * axis_jz_m[2];

			double norm = std::sqrt(x2*x2 + y2*y2);

			const_cast<double*>(imp_->loc_cm_I)[15] = -y2 / norm;
			const_cast<double*>(imp_->loc_cm_I)[19] = x2 / norm;

			return imp_->loc_cm_I;
		}
		auto UniversalJoint::cptCa(double *ca)const->void
		{
			Constraint::cptCa(ca);

			double tem[6];

			// update makI的z轴 和 makJ的z轴
			const double axis_i_m[3]{ makI().prtPm()[0][2] ,makI().prtPm()[1][2] ,makI().prtPm()[2][2] };
			double axis_j_m[3];
			s_pm_dot_v3(*makJ().fatherPart().pm(), &makJ().prtPm()[0][2], 4, tem, 1);
			s_inv_pm_dot_v3(*makI().fatherPart().pm(), tem, axis_j_m);

			// compute c_dot //
			double wm_in_m[3], wn_in_m[3];
			s_inv_pm_dot_v3(*makI().fatherPart().pm(), makI().fatherPart().vs() + 3, wm_in_m);
			s_inv_pm_dot_v3(*makI().fatherPart().pm(), makJ().fatherPart().vs() + 3, wn_in_m);

			double iwm = s_vv(3, axis_i_m, wm_in_m);
			double jwm = s_vv(3, axis_j_m, wm_in_m);
			double iwn = s_vv(3, axis_i_m, wn_in_m);
			double jwn = s_vv(3, axis_j_m, wn_in_m);

			ca[3] += 2 * jwm*iwn - jwm*iwm - jwn*iwn;
		}
		auto UniversalJoint::cptCpFromPm(double *cp, const double *makI_pm, const double *makJ_pm)const->void
		{
			double pm_j_in_i[16];
			s_inv_pm_dot_pm(makI_pm, makJ_pm, pm_j_in_i);

			cp[0] = pm_j_in_i[3];
			cp[1] = pm_j_in_i[7];
			cp[2] = pm_j_in_i[11];

			// 两个坐标系的z轴的角度差应该为90度
			cp[3] = -PI / 2.0 + std::acos(pm_j_in_i[10]);
		}
		UniversalJoint::~UniversalJoint() = default;
		UniversalJoint::UniversalJoint(const std::string &name, Marker* makI, Marker* makJ) : Joint(name, makI, makJ), imp_(new Imp)
		{
			const static double loc_cst[6][4]
			{
				1,0,0,0,
				0,1,0,0,
				0,0,1,0,
				0,0,0,0,
				0,0,0,0,
				0,0,0,0,
			};
			s_mc(6, dim(), *loc_cst, imp_->loc_cm_I);
		}
		UniversalJoint::UniversalJoint(const UniversalJoint &other) = default;
		UniversalJoint::UniversalJoint(UniversalJoint &&other) = default;
		UniversalJoint& UniversalJoint::operator=(const UniversalJoint &other) = default;
		UniversalJoint& UniversalJoint::operator=(UniversalJoint &&other) = default;

		auto SphericalJoint::locCmI() const->const double*
		{
			static const double loc_cm_I[18]
			{
				1,0,0,
				0,1,0,
				0,0,1,
				0,0,0,
				0,0,0,
				0,0,0,
			};
			return loc_cm_I;
		}
		auto SphericalJoint::cptCpFromPm(double *cp, const double *makI_pm, const double *makJ_pm)const->void
		{
			/////////////////////////以下是pa的计算方法///////////////////////////
			double pp_j[3]{ makJ_pm[3], makJ_pm[7], makJ_pm[11], };
			s_inv_pp2pp(makI_pm, pp_j, cp);
			/////////////////////////以上是pa的计算方法///////////////////////////
		}
		SphericalJoint::SphericalJoint(const std::string &name, Marker* makI, Marker* makJ): Joint(name, makI, makJ){}

		auto SingleComponentForce::saveXml(aris::core::XmlElement &xml_ele) const->void
		{
			Force::saveXml(xml_ele);
			xml_ele.SetAttribute("component", static_cast<int>(this->component_axis_));
		}
		auto SingleComponentForce::loadXml(const aris::core::XmlElement &xml_ele)->void
		{
			component_axis_ = attributeInt32(xml_ele, "component", 0);

			Force::loadXml(xml_ele);
		}
		auto SingleComponentForce::cptGlbFs(double *fsI, double *fsJ)->void
		{
			s_tf(*makI().prtPm(), fce_value_, fsJ);
			s_tf(*makI().fatherPart().pm(), fsJ, fsI);
			s_vi(6, fsI, fsJ);
		}
		SingleComponentForce::SingleComponentForce(const std::string &name, Marker* makI, Marker* makJ, Size componentID) : Force(name, makI, makJ), component_axis_(componentID) {}
	}
}
