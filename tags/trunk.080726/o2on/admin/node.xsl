<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
	<xsl:template match="/">
		<xsl:apply-templates select="nodes"/>
	</xsl:template>
	<xsl:template match="nodes">
	  <div id="body" class="body">
	    <span style="font-size:75%">
        DHT: <b><xsl:value-of select="info/id"/></b> 付近のノードを担当
      </span>
      <div class="infomessage {info/message_type}">
        <xsl:value-of select="info/message"/>
      </div>
    	<table width="100%">
    	  <tr>
    	    <td class="layout" valign="bottom" width="33%">
          	<span>
              <xsl:value-of select="info/count"/>
          	</span>
          </td>
          <td class="layout" valign="bottom" width="34%">
        		<form name="h" onSubmit="SetLimit(); return false;">
              表示上限:
          		<input type="text" name="lim" class="lim" value=""/>
          		<input type="button" class="btn" value="再表示" onClick="SetLimit()"/>
        		</form>
          </td>
          <td class="layout" valign="bottom" align="right" width="33%">
          	<input type="button" class="btn" value="追加" onClick="showIniNodeForm()"/>
          	<input type="button" value="URLから追加" onClick="showURLIniNodeForm()"/>
          	<input type="button" value="自ノード情報" onClick="showEncProfForm()"/>
          </td>
    	  </tr>
    	</table>
      <table width="100%"><tr>
        <xsl:for-each select="info/kbucket">
          <td class="kb" style="background-color:rgb({100-.}%,100%,100%)"></td>
        </xsl:for-each>
      </tr></table>
    	<table width="100%">
      	<tr>
        	<th rowspan="2"></th>
        	<th class="sort" rowspan="2" onClick="Sort('distance','text')">d</th>
        	<th class="sort" rowspan="2" onClick="Sort('name','text')">Name</th>
        	<th class="sort" rowspan="2" onClick="Sort('flags','text')">flg</th>
        	<th class="sort" rowspan="2" onClick="Sort('ip','text')">IP(e)</th>
        	<th class="sort" rowspan="2" onClick="Sort('port','number')">Port</th>
        	<th class="sort" rowspan="2" onClick="Sort('ua','text')">UA</th>
        	<th class="sort" colspan="2">自分から接続</th>
        	<th class="sort" colspan="2">相手から接続</th>
        	<th class="sort" rowspan="2" onClick="Sort('lastmodified','text')">最終更新</th>
        	<th class="sortL" rowspan="2" onClick="Sort('id','text')">ID</th>
      	</tr>
      	<tr>
        	<th class="sort" onClick="Sort('recvbyte_me2n','number')">受信byte</th>
        	<th class="sort" onClick="Sort('sendbyte_me2n','number')">送信byte</th>
        	<th class="sort" onClick="Sort('recvbyte_n2me','number')">受信byte</th>
        	<th class="sort" onClick="Sort('sendbyte_n2me','number')">送信byte</th>
      	</tr>
  			<xsl:apply-templates select="node">
          <xsl:sort select="SORTKEY" data-type="SORTTYPE" order="SORTORDER"/>
        </xsl:apply-templates>
  		</table>
    </div>
	</xsl:template>
	<xsl:template match="node">
	  <xsl:param name="limit" select="LIMIT"/>
    <xsl:if test="position() &lt; $limit">
  		<tr>
  			<td>
  				<!--xsl:if test="status/@linkedfrom or status/@pastlinkedfrom or status/@linkedto or status/@pastlinkedto">
    			  <img src="/images/mypc.png" width="10" height="13"/>
  				</xsl:if-->
  				<xsl:if test="status/@linkedfrom = '1'">
  					<span class="linked">←</span>
  				</xsl:if>
  				<xsl:if test="(not(status/@linkedfrom) or status/@linkedfrom != '1') and status/@pastlinkedfrom = '1'">
  					<span class="past">←</span>
  				</xsl:if>
  				<xsl:if test="status/@linkedto = '1'">
  					<span class="linked">→</span>
  				</xsl:if>
  				<xsl:if test="(not(status/@linkedto) or status/@linkedto != '1') and status/@pastlinkedto = '1'">
  					<span class="past">→</span>
  				</xsl:if>
  			</td>
  			<td class="C">
  				<xsl:value-of select="distance"/>
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
  			<td class="monospace">
          <xsl:value-of select="flags"/>
  			</td>
  			<td class="C">
          <xsl:value-of select="ip"/>
  			</td>
  			<td class="R">
  				<xsl:value-of select="port"/>
  			</td>
  			<td>
  				<xsl:value-of select="ua"/>
  			</td>
  			<td class="R">
  				<xsl:value-of select="format-number(recvbyte_me2n, '###,###,###,###')"/>
  			</td>
  			<td class="R">
  				<xsl:value-of select="format-number(sendbyte_me2n, '###,###,###,###')"/>
  			</td>
  			<td class="R">
  				<xsl:value-of select="format-number(recvbyte_n2me, '###,###,###,###')"/>
  			</td>
  			<td class="R">
  				<xsl:value-of select="format-number(sendbyte_n2me, '###,###,###,###')"/>
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
