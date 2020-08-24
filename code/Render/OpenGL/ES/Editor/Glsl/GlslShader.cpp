#include "Core/Settings/PropertyBoolean.h"
#include "Core/Settings/PropertyGroup.h"
#include "Render/OpenGL/ES/Editor/Glsl/GlslShader.h"

namespace traktor
{
	namespace render
	{

GlslShader::GlslShader(ShaderType shaderType)
:	m_shaderType(shaderType)
,	m_nextTemporaryVariable(0)
{
	pushScope();
	pushOutputStream(BtUniform, new StringOutputStream());
	pushOutputStream(BtInput, new StringOutputStream());
	pushOutputStream(BtOutput, new StringOutputStream());
	pushOutputStream(BtScript, new StringOutputStream());
	pushOutputStream(BtBody, new StringOutputStream());
}

GlslShader::~GlslShader()
{
	popOutputStream(BtBody);
	popOutputStream(BtScript);
	popOutputStream(BtOutput);
	popOutputStream(BtInput);
	popOutputStream(BtUniform);
	popScope();
}

void GlslShader::addInputVariable(const std::wstring& variableName, GlslVariable* variable)
{
	T_ASSERT(!m_inputVariables[variableName]);
	m_inputVariables[variableName] = variable;
}

GlslVariable* GlslShader::getInputVariable(const std::wstring& variableName)
{
	return m_inputVariables[variableName];
}

GlslVariable* GlslShader::createTemporaryVariable(const OutputPin* outputPin, GlslType type)
{
	StringOutputStream ss;
	ss << L"v" << m_nextTemporaryVariable++;
	return createVariable(outputPin, ss.str(), type);
}

GlslVariable* GlslShader::createVariable(const OutputPin* outputPin, const std::wstring& variableName, GlslType type)
{
	T_ASSERT(!m_variables.empty());

	Ref< GlslVariable > variable = new GlslVariable(variableName, type);
	m_variables.back().insert(std::make_pair(outputPin, variable));

	return variable;
}

GlslVariable* GlslShader::createOuterVariable(const OutputPin* outputPin, const std::wstring& variableName, GlslType type)
{
	T_ASSERT(!m_variables.empty());

	Ref< GlslVariable > variable = new GlslVariable(variableName, type);
	m_variables.front().insert(std::make_pair(outputPin, variable));

	return variable;
}

void GlslShader::associateVariable(const OutputPin* outputPin, GlslVariable* variable)
{
	m_variables.back().insert(std::make_pair(outputPin, variable));
}

GlslVariable* GlslShader::getVariable(const OutputPin* outputPin)
{
	T_ASSERT(!m_variables.empty());

	for (std::list< scope_t >::reverse_iterator i = m_variables.rbegin(); i != m_variables.rend(); ++i)
	{
		scope_t::iterator j = i->find(outputPin);
		if (j != i->end())
			return j->second;
	}

	return 0;
}

void GlslShader::pushScope()
{
	m_variables.push_back(scope_t());
}

void GlslShader::popScope()
{
	if (!m_variables.empty())
		m_variables.pop_back();
}

void GlslShader::addUniform(const std::wstring& uniform)
{
	m_uniforms.insert(uniform);
}

const std::set< std::wstring >& GlslShader::getUniforms() const
{
	return m_uniforms;
}

bool GlslShader::defineScript(const std::wstring& signature)
{
	std::set< std::wstring >::iterator i = m_scriptSignatures.find(signature);
	if (i != m_scriptSignatures.end())
		return false;

	m_scriptSignatures.insert(signature);
	return true;
}

void GlslShader::pushOutputStream(BlockType blockType, StringOutputStream* outputStream)
{
	m_outputStreams[int(blockType)].push_back(outputStream);
}

void GlslShader::popOutputStream(BlockType blockType)
{
	if (!m_outputStreams[int(blockType)].empty())
		m_outputStreams[int(blockType)].pop_back();
}

StringOutputStream& GlslShader::getOutputStream(BlockType blockType)
{
	T_ASSERT(!m_outputStreams[int(blockType)].empty());
	return *(m_outputStreams[int(blockType)].back());
}

const StringOutputStream& GlslShader::getOutputStream(BlockType blockType) const
{
	T_ASSERT(!m_outputStreams[int(blockType)].empty());
	return *(m_outputStreams[int(blockType)].back());
}

std::wstring GlslShader::getGeneratedShader(const PropertyGroup* settings, const std::wstring& name, const GlslRequirements& requirements) const
{
	StringOutputStream ss;

	if (m_shaderType == StFragment && requirements.derivatives)
		ss << L"#extension GL_OES_standard_derivatives : enable" << Endl;

	if (m_shaderType == StFragment && requirements.texture3D)
		ss << L"#extension GL_OES_texture_3D : enable" << Endl;

	if (m_shaderType == StFragment && requirements.shadowSamplers)
		ss << L"#extension GL_EXT_shadow_samplers : require" << Endl;

	if (settings && settings->getProperty< bool >(L"Glsl.ES2.SupportHwInstancing", false))
		ss << L"#extension GL_EXT_draw_instanced : enable" << Endl;

	ss << Endl;

	ss << L"// THIS SHADER IS AUTOMATICALLY GENERATED! DO NOT EDIT!" << Endl;
	if (!name.empty())
		ss << L"// " << name << Endl;
	ss << Endl;

	ss << L"precision highp float;" << Endl;
	ss << Endl;

	ss << L"uniform vec4 _gl_targetSize;" << Endl;
	if (!settings || !settings->getProperty< bool >(L"Glsl.ES2.SupportHwInstancing", false))
		ss << L"uniform float _gl_instanceID;" << Endl;
	ss << Endl;

	switch (requirements.precisionHint)
	{
	case PhLow:
		ss << L"precision lowp float;" << Endl;
		ss << Endl;
		break;

	case PhMedium:
		ss << L"precision mediump float;" << Endl;
		ss << Endl;
		break;

	case PhHigh:
	case PhUndefined:
		ss << L"precision highp float;" << Endl;
		ss << Endl;
		break;
	}

	if (m_shaderType == StVertex)
	{
		// Add post-orientation transform function.
		ss << L"uniform vec4 _gl_postTransform;" << Endl;
		ss << Endl;

		ss << L"vec4 PV(in vec4 cp0)" << Endl;
		ss << L"{" << Endl;
		ss << L"\treturn vec4(" << Endl;
		ss << L"\t\tcp0.x * _gl_postTransform.x + cp0.y * _gl_postTransform.y," << Endl;
		ss << L"\t\tcp0.x * _gl_postTransform.z + cp0.y * _gl_postTransform.w," << Endl;
		ss << L"\t\tcp0.z," << Endl;
		ss << L"\t\tcp0.w" << Endl;
		ss << L"\t);" << Endl;
		ss << L"}" << Endl;
		ss << Endl;
	}

	// Add transpose function; not implemented by default in GLSL 1.0
	if (requirements.transpose)
	{
		ss << L"mat4 transpose(in mat4 m)" << Endl;
		ss << L"{" << Endl;
		ss << L"\treturn mat4(" << Endl;
		ss << L"\t\tm[0][0], m[1][0], m[2][0], m[3][0]," << Endl;
		ss << L"\t\tm[0][1], m[1][1], m[2][1], m[3][1]," << Endl;
		ss << L"\t\tm[0][2], m[1][2], m[2][2], m[3][2]," << Endl;
		ss << L"\t\tm[0][3], m[1][3], m[2][3], m[3][3]" << Endl;
		ss << L"\t);" << Endl;
		ss << L"}" << Endl;
		ss << Endl;
	}

	ss << getOutputStream(BtUniform).str();
	ss << Endl;
	ss << getOutputStream(BtInput).str();
	ss << Endl;
	ss << getOutputStream(BtOutput).str();
	ss << Endl;
	ss << getOutputStream(BtScript).str();
	ss << Endl;
	ss << L"void main()" << Endl;
	ss << L"{" << Endl;
	ss << IncreaseIndent;
	ss << getOutputStream(BtBody).str();
	ss << DecreaseIndent;
	ss << L"}" << Endl;

	return ss.str();
}

	}
}