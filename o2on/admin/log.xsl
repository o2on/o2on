<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
	<xsl:template match="/">
		<xsl:apply-templates select="logs"/>
	</xsl:template>
	<xsl:template match="logs">
	  <div id="body" class="body">
      <div class="infomessage {info/message_type}">
        <xsl:value-of select="info/message"/>
      </div>
    	<table width="100%">
    	  <tr>
    	    <td class="layout" valign="bottom" width="33%">
          	<span>
                <xsl:value-of select="info/count"/>
                /
                <xsl:value-of select="info/limit"/>
          	</span>
          </td>
          <td class="layout" width="34%">
       		  <input type="button" class="button" value="　　再表示　　" onClick="Reload()"/>
          </td>
          <td class="layout" width="33%" align="right">
          </td>
    	  </tr>
    	</table>
    	<table width="100%">
      	<tr>
        	<th class="sort" onClick="Sort('date','text')">日時</th>
        	<th class="sort" onClick="Sort('module','text')">モジュール</th>
        	<th class="sort" onClick="Sort('type','text')">タイプ</th>
        	<th class="sort" onClick="Sort('ip','text')">IP</th>
        	<th class="sort" onClick="Sort('port','text')">Port</th>
        	<th class="sort" onClick="Sort('msg','text')">内容</th>
      	</tr>
  			<xsl:apply-templates select="log">
            <xsl:sort select="SORTKEY" data-type="SORTTYPE" order="SORTORDER"/>
        </xsl:apply-templates>
  		</table>
    </div>
	</xsl:template>
	<xsl:template match="log">
	  <xsl:param name="limit" select="LIMIT"/>
    <xsl:if test="position() &lt; $limit">
  		<tr>
  			<td>
  				<xsl:value-of select="date"/>
  			</td>
  			<td class="{type}">
  			 	<xsl:value-of select="module"/>
  			</td>
  			<td class="{type}">
  				<xsl:value-of select="type"/>
  			</td>
  			<td class="{type}">
  				<xsl:value-of select="ip"/>
  			</td>
  			<td class="{type}">
  				<xsl:value-of select="port"/>
  			</td>
  			<td class="{type}">
  				<xsl:value-of select="msg"/>
  			</td>
  		</tr>
    </xsl:if>
	</xsl:template>
</xsl:stylesheet>
