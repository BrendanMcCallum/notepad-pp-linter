#include "stdafx.h"
#include "XmlParser.h"
#include "encoding.h"

#include <msxml6.h>
#pragma comment(lib, "msxml6.lib")

#define RELEASE(iface) \
  if (iface)           \
  {                    \
    iface->Release();  \
    iface = NULL;      \
  }

std::vector<XmlParser::Error> XmlParser::getErrors(const std::string &xml)
{
  std::vector<XmlParser::Error> errors;
  IXMLDOMNodeList *XMLNodeList(NULL);
  IXMLDOMNode *XMLNode(NULL);
  IXMLDOMDocument2 *XMLDocument(NULL);
  HRESULT hr;

  try
  {
    hr = CoCreateInstance(__uuidof(DOMDocument), NULL, CLSCTX_SERVER, IID_IXMLDOMDocument2, (LPVOID *)(&XMLDocument));
    if (!SUCCEEDED(hr) || !XMLDocument)
    {
      throw ::Linter::Exception("Linter: Can't create IID_IXMLDOMDocument2");
    }

    hr = XMLDocument->put_async(VARIANT_FALSE);
    if (!SUCCEEDED(hr))
    {
      throw ::Linter::Exception("Linter: Can't XMLDOMDocument2::put_async");
    }

    std::wstring &string = Encoding::toUnicode(xml);
    BSTR bstrValue(const_cast<wchar_t *>(string.c_str()));

    short resultCode = FALSE;
    hr = XMLDocument->loadXML(bstrValue, &resultCode);
    if (!SUCCEEDED(hr) || (resultCode != VARIANT_TRUE))
    {
      throw ::Linter::Exception("Linter: Invalid output format. Only checkstyle-compatible output allowed.");
    }

    // <error line="12" column="19" severity="error" message="Unexpected identifier" source="jscs" />
    hr = XMLDocument->selectNodes(_T("//error"), &XMLNodeList);
    if (!SUCCEEDED(hr))
    {
      throw ::Linter::Exception("Linter: Can't execute XPath //error");
    }
    LONG uLength;

    hr = XMLNodeList->get_length(&uLength);
    if (!SUCCEEDED(hr))
    {
      throw ::Linter::Exception("Linter: Can't get XPath //error length");
    }
    for (int iIndex = 0; iIndex < uLength; iIndex++)
    {
      IXMLDOMNode *node;
      hr = XMLNodeList->nextNode(&node);
      if (!SUCCEEDED(hr))
      {
        throw ::Linter::Exception("Linter: Can't get next XPath element");
      }
      CComQIPtr<IXMLDOMElement> element(node);
      if (!SUCCEEDED(hr) && element)
      {
        throw ::Linter::Exception("Linter: XPath Element error");
      }
      Error error;
      CComVariant value;

      element->getAttribute(L"line", &value);
      error.m_line = _wtoi(value.bstrVal);

      element->getAttribute(L"column", &value);
      error.m_column = _wtoi(value.bstrVal);

      element->getAttribute(L"message", &value);
      error.m_message = value.bstrVal;

      errors.push_back(error);
      RELEASE(node);
    }
    RELEASE(XMLNodeList);
    RELEASE(XMLDocument);
  }
  catch (::Linter::Exception &)
  {
    RELEASE(XMLNode);
    RELEASE(XMLDocument);
    RELEASE(XMLNodeList);

    throw;
  }
  catch (...)
  {
    RELEASE(XMLNode);
    RELEASE(XMLDocument);
    RELEASE(XMLNodeList);
  }

  return errors;
}

XmlParser::Settings XmlParser::getLinters(std::wstring file)
{
  XmlParser::Settings settings;
  IXMLDOMNodeList *XMLNodeList(NULL), *styleNode(NULL);
  IXMLDOMNode *XMLNode(NULL);
  IXMLDOMDocument2 *XMLDocument(NULL);
  HRESULT hr;
  LONG uLength;

  try
  {
    hr = CoCreateInstance(__uuidof(DOMDocument), NULL, CLSCTX_SERVER, IID_IXMLDOMDocument2, (LPVOID *)(&XMLDocument));
    if (!SUCCEEDED(hr) || !XMLDocument)
    {
      throw ::Linter::Exception("Linter: Can't create IID_IXMLDOMDocument2");
    }

    hr = XMLDocument->put_async(VARIANT_FALSE);
    if (!SUCCEEDED(hr))
    {
      throw ::Linter::Exception("Linter: XMLDOMDocument2::put_async error.");
    }

    BSTR bstrValue(const_cast<wchar_t *>(file.c_str()));
    CComVariant value(bstrValue);

    short resultCode = FALSE;
    hr = XMLDocument->load(value, &resultCode);
    if (!SUCCEEDED(hr) || (resultCode != VARIANT_TRUE))
    {
      throw ::Linter::Exception("Linter: linter.xml load error. Check file format.");
    }

    hr = XMLDocument->selectNodes(_T("//style"), &styleNode);
    if (!SUCCEEDED(hr))
    {
      throw ::Linter::Exception("Linter: Can't execute XPath //style");
    }

    hr = styleNode->get_length(&uLength);
    if (!SUCCEEDED(hr))
    {
      throw ::Linter::Exception("Linter: Can't get XPath root length");
    }
    else if (uLength)
    {
      IXMLDOMNode *node;
      hr = styleNode->nextNode(&node);
      if (SUCCEEDED(hr))
      {
        CComQIPtr<IXMLDOMElement> element(node);

        CComVariant value;
        if (SUCCEEDED(element->getAttribute(L"alpha", &value)))
        {
          std::wstringstream data(std::wstring(value.bstrVal ? value.bstrVal : L""));
          data >> settings.m_alpha;
        }


        if (SUCCEEDED(element->getAttribute(L"color", &value)))
        {
          std::wstringstream data(std::wstring(value.bstrVal ? value.bstrVal : L""));
          unsigned int color(0);
          data >> std::hex >> color;

          // reverse colors for scintilla's LE order
          settings.m_color = color & 0xFF;

          settings.m_color <<= 8; color >>= 8;
          settings.m_color |= color & 0xFF;

          settings.m_color <<= 8; color >>= 8;
          settings.m_color |= color & 0xFF;
        }
      }
    }

    // <error line="12" column="19" severity="error" message="Unexpected identifier" source="jscs" />
    hr = XMLDocument->selectNodes(_T("//linter"), &XMLNodeList);
    if (!SUCCEEDED(hr))
    {
      throw ::Linter::Exception("Linter: Can't execute XPath //linter");
    }

    hr = XMLNodeList->get_length(&uLength);
    if (!SUCCEEDED(hr))
    {
      throw ::Linter::Exception("Linter: Can't get XPath length");
    }

    for (int iIndex = 0; iIndex < uLength; iIndex++)
    {
      IXMLDOMNode *node;
      hr = XMLNodeList->nextNode(&node);
      if (SUCCEEDED(hr))
      {
        CComQIPtr<IXMLDOMElement> element(node);
        if (SUCCEEDED(hr) && element)
        {
          Linter linter;
          CComVariant value;

          element->getAttribute(L"extension", &value);
          linter.m_extension = value.bstrVal;

          element->getAttribute(L"command", &value);
          linter.m_command = value.bstrVal;

          element->getAttribute(L"stdin", &value);
          linter.m_useStdin = !!value.boolVal;

          settings.m_linters.push_back(linter);
        }
        RELEASE(node);
      }
    }
    RELEASE(XMLNodeList);
    RELEASE(XMLDocument);
  }
  catch (::Linter::Exception &)
  {
    RELEASE(XMLNode);
    RELEASE(XMLDocument);
    RELEASE(XMLNodeList);

    throw;
  }
  catch (...)
  {
    RELEASE(XMLNode);
    RELEASE(XMLDocument);
    RELEASE(XMLNodeList);
  }

  return settings;
}
