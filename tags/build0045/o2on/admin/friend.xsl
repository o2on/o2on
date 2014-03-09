<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
	<xsl:template match="/">
		<xsl:apply-templates select="nodes"/>
	</xsl:template>
	<xsl:template match="nodes">
    <form name="d" method="POST" action="/xml/friend">
  	  <div id="body" class="body">
        <div class="infomessage {info/message_type}">
          <xsl:value-of select="info/message"/>
        </div>
      	<table width="100%">
      	  <tr>
      	    <td class="layout" width="33%">
              <input type="button" value="全選択" onClick="checkAllItem(true)"/>
              <input type="button" value="全解除" onClick="checkAllItem(false)"/>
              <input type="button" style="width:120px" value="削除" onClick="submit()"/>
              <input type="hidden" name="act" value="delete"/>
            </td>
            <td class="layout" width="34%">
              <input type="button" class="button" value="　　再表示　　" onClick="Reload()"/>
            </td>
      	    <td class="layout" width="33%">
            </td>
      	  </tr>
      	</table>
      	<table width="100%">
        	<tr>
          	<th></th>
          	<th></th>
          	<th class="sort" onClick="Sort('name','text')">名前</th>
          	<th class="sort" onClick="Sort('date','text')">最終確認日時</th>
          	<th class="sort" onClick="Sort('id','text')">ID</th>
        	</tr>
    			<xsl:apply-templates select="node">
              <xsl:sort select="SORTKEY" data-type="SORTTYPE" order="SORTORDER"/>
          </xsl:apply-templates>
    		</table>
      </div>
    </form>
	</xsl:template>
	<xsl:template match="node">
	  <xsl:param name="limit" select="LIMIT"/>
    <xsl:if test="position() &lt; $limit">
  		<tr>
  	    <td>
  	      <input type="checkbox" class="checkbox" name="{id}" value=""/>
  	    </td>
        <td>
  				<xsl:if test="status/@linkedfrom = '1'">
  					<span class="linked">●</span>
  				</xsl:if>
  				<xsl:if test="status/@pastlinkedfrom = '1'">
  					<span class="past">●</span>
  				</xsl:if>
        </td>
  			<td>
          <xsl:if test="port = '0'">
      			<xsl:if test="not(name) or name = ''">
      			  ?
      			</xsl:if>
      			<xsl:if test="name and name != ''">
    	    		<xsl:value-of select="name"/>
      			</xsl:if>
          </xsl:if>
          <xsl:if test="port != '0'">
    			  <a href="javascript:void(0)" onClick="showMsgForm('{id}','{ip}','{port}','{name}','{pubkey}')">
      			  <xsl:if test="not(name) or name = ''">
      			    ?
      			  </xsl:if>
      			  <xsl:if test="name and name != ''">
    	    			<xsl:value-of select="name"/>
      			  </xsl:if>
            </a>
          </xsl:if>
  			</td>
  			<td>
  				<xsl:value-of select="lastlink"/>
  			</td>
  			<td>
  				<xsl:value-of select="id"/>
  			</td>
  		</tr>
    </xsl:if>
	</xsl:template>
</xsl:stylesheet>
