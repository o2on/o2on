<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
	<xsl:template match="/">
		<xsl:apply-templates select="ipf"/>
	</xsl:template>

	<xsl:template match="ipf">
    <form name="d" method="POST" action="/xml/ipf">
  	  <div id="body" class="body">
  	    <div class="caption">
          <xsl:value-of select="type"/>用IPフィルタ
        </div>
  	    <xsl:if test="message">
          <div class="infomessage {message_type}">
            <xsl:value-of select="message"/>
          </div>
  	    </xsl:if>

      	<table width="100%">
      	  <tr>
      	    <td class="layout" width="40%">
            	デフォルト:
              <select name="default">
                <xsl:if test="default = 'allow'">
                  <option value="allow" selected="1">通過</option>
                  <option value="deny">切断</option>
                </xsl:if>
                <xsl:if test="default != 'allow'">
                  <option value="allow">通過</option>
                  <option value="deny" selected="1">切断</option>
                </xsl:if>
              </select>
            </td>
      	    <td class="layout" width="20%">
              <input type="submit" style="width:100%" value="決定"/>
            </td>
      	    <td class="layout" width="40%" align="right">
      	      ※条件を空にすると削除できます
            </td>
      	  </tr>
      	</table>
      	<table width="100%">
        	<tr>
          	<th>有効</th>
          	<th>通過/切断</th>
          	<th>条件</th>
        	</tr>
          <xsl:apply-templates select="filter"/>
    		</table>
      </div>
      <input type="hidden" name="type" value="{type}"/>
    </form>
	</xsl:template>

	<xsl:template match="filter">
    <tr>
      <td width="30">
        <xsl:if test="enable = 'true'">
          <input type="checkbox" class="checkbox" name="e{position()}" value="true" checked="1"/>
        </xsl:if>
        <xsl:if test="enable != 'true'">
          <input type="checkbox" class="checkbox" name="e{position()}" value="true"/>
        </xsl:if>
      </td>
    	<td width="65">
        <select name="f{position()}">
          <xsl:if test="flag = 'allow'">
            <option value="allow" selected="1">通過</option>
            <option value="deny">切断</option>
          </xsl:if>
          <xsl:if test="flag != 'allow'">
            <option value="allow">通過</option>
            <option value="deny" selected="1">切断</option>
          </xsl:if>
        </select>
    	</td>
    	<td>
        <input type="text" style="width:100%" name="c{position()}" value="{cond}"/>
    	</td>
    </tr>
	</xsl:template>
</xsl:stylesheet>
