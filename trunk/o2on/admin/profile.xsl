<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
	<xsl:template match="/">
    <html>
      <head>
      	<link rel="stylesheet" type="text/css" href="../style.css"/>
      </head>
      <body class="font100">
    		<xsl:apply-templates select="report"/>
      </body>
    </html>
	</xsl:template>
	<xsl:template match="report">
	  <div id="body" class="marginbody">
      <xsl:if test="message">
        <div class="marginbody">
          <xsl:value-of select="message"/>
        </div>
  	  </xsl:if>
      <xsl:if test="name and name != ''">
        <div class="caption">
          <xsl:value-of select="name"/>
        </div>
  	  </xsl:if>
      <xsl:if test="name and name = ''">
        <div class="caption">
          名無しさん
        </div>
  	  </xsl:if>
  		<xsl:apply-templates select="category"/>
    </div>
	</xsl:template>

	<xsl:template match="category">
    <div class="category">
      <xsl:if test="caption and caption != ''">
        <div class="caption">
          <xsl:value-of select="caption"/>
        </div>
      </xsl:if>
      <xsl:apply-templates select="table"/>
    </div>
	</xsl:template>

	<xsl:template match="table">
	  <table class="report">
      <xsl:apply-templates select="tr"/>
	  </table>
	</xsl:template>
	<xsl:template match="tr">
	  <tr>
      <xsl:apply-templates select="td"/>
      <xsl:apply-templates select="pre"/>
	  </tr>
	</xsl:template>

	<xsl:template match="td">
    <xsl:if test="@type and @type = 'h'">
  	  <th class="{@class}">
        <xsl:value-of select="."/>
  	  </th>
    </xsl:if>
    <xsl:if test="not(@type) or @type != 'h'">
      <td class="{@class}">
        <xsl:if test="@class and @class = 'N'">
    				<xsl:value-of select="format-number(., '###,###,###,##0.##')"/>
        </xsl:if>
        <xsl:if test="not(@class) or @class != 'N'">
            <xsl:value-of select="."/>
        </xsl:if>
      </td>
    </xsl:if>
	</xsl:template>

	<xsl:template match="pre">
    <td>
      <pre>
        <xsl:value-of select="."/>
      </pre>
    </td>
	</xsl:template>
</xsl:stylesheet>
