<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
	<xsl:template match="/">
		<xsl:apply-templates select="report"/>
	</xsl:template>
	<xsl:template match="report">
	  <br/>
	  <div id="body" class="body">
    	<table width="100%">
    	  <tr>
    	    <td class="layout" valign="bottom" width="40%">
          </td>
          <td class="layout" valign="bottom" width="20%">
       		  <input type="button" class="button" value="　　再表示　　" onClick="Reload()"/>
          </td>
          <td class="layout" valign="bottom" align="right" width="40%">
            <input type="button" value="自分の公開プロフィールを見る"
              onClick="window.open('/xml/profile','_blank')"/>
          </td>
    	  </tr>
    	</table>
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
</xsl:stylesheet>
