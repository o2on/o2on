<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:variable name="limit"  select="LIMIT"/>

	<xsl:template match="/">
		<xsl:apply-templates select="keys"/>
	</xsl:template>
	<xsl:template match="keys">
	  <div id="body" class="body">
	    <span style="font-size:75%">
        DHT: <b><xsl:value-of select="info/id"/></b> 付近のキーを担当
      </span>
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
        		<form name="h" onSubmit="SetLimit(); return false;">
              表示上限:
          		<input type="text" name="lim" class="lim" value=""/>
          		<input type="button" class="btn" value="再表示" onClick="SetLimit()"/>
        		</form>
          </td>
          <td class="layout" width="33%" align="right">
          </td>
    	  </tr>
    	</table>
    	<table width="100%">
      	<tr>
        	<th class="sort" onClick="Sort('distance','number')">d</th>
        	<th class="sort" onClick="Sort('ip','text')">IP(e)</th>
        	<th class="sort" onClick="Sort('port','number')">port</th>
        	<th class="sort" onClick="Sort('url','text')">URL</th>
        	<th class="sort" onClick="Sort('title','text')">title</th>
        	<th class="sort" onClick="Sort('note','text')">note</th>
        	<th class="sort" onClick="Sort('size','number')">size</th>
        	<th class="sort" onClick="Sort('date','text')">date</th>
        	<th class="sort" onClick="Sort('keyhash','text')">hash</th>
      	</tr>
  			<xsl:apply-templates select="key">
          <xsl:sort select="SORTKEY" data-type="SORTTYPE" order="SORTORDER"/>
        </xsl:apply-templates>
  		</table>
    </div>
	</xsl:template>
	<xsl:template match="key">
    <xsl:if test="position() &lt; $limit">
  		<tr>
  			<td>
  				<xsl:value-of select="distance"/>
  			</td>
  			<td class="C">
    			<a href="javascript:void(0)" onClick="showMsgForm('{nodeid}','{ip}','{port}','{name}','{pubkey}')">
    	    	<xsl:value-of select="ip"/>
          </a>
  			</td>
  			<td class="R">
  				<xsl:value-of select="port"/>
  			</td>
  			<td onClick="selectText(this)">
  				<xsl:value-of select="url"/>
  			</td>
  			<td>
  				<xsl:value-of select="title"/>
  			</td>
  			<td>
  				<xsl:value-of select="note"/>
  			</td>
  			<td class="R">
  				<xsl:value-of select="format-number(size, '###,###,###,###')"/>
  			</td>
  			<td>
  				<xsl:value-of select="date"/>
  			</td>
  			<td>
  				<xsl:value-of select="hash"/>
  			</td>
  		</tr>
    </xsl:if>
	</xsl:template>
</xsl:stylesheet>
